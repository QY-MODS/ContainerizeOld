#pragma once
#include "DynamicFormTracker.h"


using namespace Utilities::Types;

struct Source {

    bool cloud_storage;
    std::map<FormID,Count> initial_items;
    float capacity;
    std::uint32_t formid;
    std::string editorid;
    SourceData data;

    Source(const std::uint32_t id, const std::string id_str, const float capacity, const bool cs)
        : formid(id), editorid(id_str), capacity(capacity), cloud_storage(cs) {
        logger::trace("Creating source with formid: {}, editorid: {}, capacity: {}, cloud storage: {}", formid, editorid, capacity, cs);
        if (!formid) {
            logger::trace("Formid is not found. Attempting to find formid for editorid {}.", editorid);
            auto form = RE::TESForm::LookupByEditorID(editorid);

            if (form) formid = form->GetFormID();
            else logger::info("Could not find formid for editorid {}", editorid);
        }
    };
    
    std::string_view GetName() const {
        logger::trace("Getting name for formid: {}", formid);
        auto form = Utilities::FunctionsSkyrim::GetFormByID(formid, editorid);
        if (form) return form->GetName();
        else return "";
    };

    RE::TESBoundObject* GetBoundObject() const {return Utilities::FunctionsSkyrim::GetFormByID<RE::TESBoundObject>(formid, editorid);};

    void AddInitialItem(const FormID form_id, const Count count) {
        logger::trace("Adding initial item with formid: {} and count: {}", form_id, count);
        initial_items[form_id] = count;
	};
};


namespace Settings {


    constexpr auto path = L"Data/SKSE/Plugins/Containerize.ini";

    //constexpr std::uint32_t kSerializationVersion = 729; // < 0.7
    //constexpr std::uint32_t kSerializationVersion = 730; // = 0.7.0
    constexpr std::uint32_t kSerializationVersion = 731; // >= 0.7.1
    constexpr std::uint32_t kDataKey = 'CTRZ';
    constexpr std::uint32_t kDFDataKey = 'DCTZ';
    bool is_pre_0_7_1 = false;

    bool po3installed = false;

    constexpr std::array<const char*, 3> InISections = {"Containers",
                                                        "Capacities",
                                                        "Other Stuff"};
    constexpr std::array<const char*, 2> InIDefaultKeys = {"container1", "container1"};
    constexpr std::array<const char*, 2> InIDefaultVals = {"", ""};

    const std::array<std::string, 3> section_comments =
        {";Make sure to use unique keys, e.g. src1=... NOTsrc1=...",
         std::format(";Make sure to use matching keys with the ones provided in section {}.",
                     static_cast<std::string>(InISections[0])),
        ";Other gameplay related settings",
        };


    const size_t otherstuffSize = 5;
    const std::array<std::string, otherstuffSize> os_comments =
		{";Set to false to suppress the 'INI changed between saves' message.",
		"; Set to true to remove the initial carry weight bonuses on your container items.",
        "; Set to true to return to the initial menu after closing your container's menu (which you had opened by holding equip).",
        "; Set to true to sell your container to vendors together with the items inside it.",
        "; Set to true to make your containers weigh nothing by default.",
		};
   
    constexpr std::array<const char*, otherstuffSize> otherstuffKeys = 
    {"INI_changed_msg", "RemoveCarryBoosts","ReturnToInitialMenu", "BatchSell", "CloudStorage"};
    constexpr std::array<bool, otherstuffSize> otherstuffVals = {true, true, true, true, false};

    bool cloud_storage_enabled = otherstuffVals[4];
    
    std::vector<Source> LoadINISources() {
        
        logger::info("Loading ini settings: Sources");

        std::vector<Source> sources;

        // Check if powerofthree's Tweaks is installed
        if (Utilities::IsPo3Installed()) {
            logger::info("powerofthree's Tweaks is installed. Enabling EditorID support.");
            po3installed = true;
        } else {
            logger::info("powerofthree's Tweaks is not installed. Disabling EditorID support.");
            po3installed = false;
        }


        CSimpleIniA ini;
        CSimpleIniA::TNamesDepend source_names;
        CSimpleIniA::TNamesDepend otherkeys;

        ini.SetUnicode();
        ini.LoadFile(path);

        // Create Sections with defaults if they don't exist
        for (int i = 0; i < 2; ++i) {
            logger::info("Checking section {}", InISections[i]);
            if (!ini.SectionExists(InISections[i])) {
                logger::info("Section {} does not exist. Creating it.", InISections[i]);
                ini.SetValue(InISections[i], nullptr, nullptr);
                logger::info("Setting default keys for section {}", InISections[i]);
                ini.SetValue(InISections[i], InIDefaultKeys[i], InIDefaultVals[i], section_comments[i].c_str());
                logger::info("Default values set for section {}", InISections[i]);
            }
        }

        // Create Sections with defaults if they don't exist
        if (!ini.SectionExists(InISections[2])) {
            ini.SetBoolValue(InISections[2], otherstuffKeys[0], otherstuffVals[0], section_comments[2].c_str());
            logger::info("Default values set for section {}", InISections[2]);
        }

        ini.GetAllKeys(InISections[2], otherkeys);
        auto numOthers = otherkeys.size();
        logger::info("otherkeys size {}", numOthers);

        if (numOthers == 0 || numOthers != otherstuffKeys.size()) {
            logger::warn(
                "No other settings found in the ini file or Invalid number of other settings . Using defaults.");
            size_t index = 0;
            for (const auto& key : otherstuffKeys) {
				ini.SetBoolValue(InISections[2], key, otherstuffVals[index], os_comments[index].c_str());
				index++;
			}
        }


        // Sections: Containers, Capacities
        ini.GetAllKeys(InISections[0], source_names);
        auto numSources = source_names.size();
        logger::info("source_names size {}", numSources);

        sources.reserve(numSources);
        const char* val1;
        const char* val2;
        uint32_t id;
        std::string id_str;

        cloud_storage_enabled = ini.GetBoolValue(InISections[2], otherstuffKeys[4]);

        for (CSimpleIniA::TNamesDepend::const_iterator it = source_names.begin(); it != source_names.end(); ++it) {
            logger::info("source name {}", it->pItem);
            val1 = ini.GetValue(InISections[0], it->pItem);
            val2 = ini.GetValue(InISections[1], it->pItem);
            if (!val1 || !val2 || !std::strlen(val1) || !std::strlen(val2)) {
                logger::warn("Source {} is missing a value. Skipping.", it->pItem);
                continue;
            }
            logger::info("Source {} has a value of {}", it->pItem, val1);
            logger::info("We have valid entries for container: {} and capacity: {}", val1, val2);
            // back to container_id and capacity
            id = static_cast<uint32_t>(std::strtoul(val1, nullptr, 16));
            id_str = std::string(val1);

            // if both formid is valid hex, use it
            if (Utilities::isValidHexWithLength7or8(val1)) {
                logger::info("Formid {} is valid hex", val1);
                sources.emplace_back(id, "", std::stof(val2), cloud_storage_enabled);
            }
            else if (!po3installed) {
                logger::error("No formid AND powerofthree's Tweaks is not installed.", val1);
                Utilities::MsgBoxesNotifs::Windows::Po3ErrMsg();
                return sources;
            } 
            else sources.emplace_back(0, id_str, std::stof(std::string(val2)), cloud_storage_enabled);

            logger::trace("Source {} has a value of {}", it->pItem, val1);
            ini.SetValue(InISections[0], it->pItem, val1);
            ini.SetValue(InISections[1], it->pItem, val2);
            logger::info("Loaded container: {} with capacity: {}", val1, std::stof(val2));
        }

        ini.SaveFile(path);

        return sources;
    };

    Source _parseSource(const YAML::Node& config) {

        auto temp_formeditorid = config["FormEditorID"] && !config["FormEditorID"].IsNull() ? config["FormEditorID"].as<std::string>() : "";
        FormID temp_formid = temp_formeditorid.empty() ? 0 : Utilities::FunctionsSkyrim::GetFormEditorIDFromString(temp_formeditorid);
        const auto temp_weight_limit = config["weight_limit"] && !config["weight_limit"].IsNull() ? config["weight_limit"].as<float>() : 0.f;
        const auto cloud_storage = config["cloud_storage"] && !config["cloud_storage"].IsNull() ? config["cloud_storage"].as<bool>() : cloud_storage_enabled;
        logger::trace("FormEditorID: {}, FormID: {}, WeightLimit: {}, CloudStorage: {}", temp_formeditorid, temp_formid, temp_weight_limit, cloud_storage);
        Source source(temp_formid, "", temp_weight_limit, cloud_storage);

        if (!config["initial_items"] || config["initial_items"].size() == 0) {
            logger::info("initial_items are empty.");
            return source;
        }
        for (const auto& itemNode : config["initial_items"]) {
            temp_formeditorid = itemNode["FormEditorID"] && !itemNode["FormEditorID"].IsNull()
                                               ? itemNode["FormEditorID"].as<std::string>()
                                               : "";

            temp_formid = temp_formeditorid.empty() ? 0 : Utilities::FunctionsSkyrim::GetFormEditorIDFromString(temp_formeditorid);
            if (!temp_formid && !temp_formeditorid.empty()) {
                logger::error("Formid could not be obtained for {}", temp_formid, temp_formeditorid);
                continue;
            }
            if (!itemNode["count"] || itemNode["count"].IsNull()) {
                logger::error("Count is null.");
                continue;
            }
            logger::trace("Count");
            const Count temp_count = itemNode["count"].as<Count>();
            if (temp_count == 0) {
                logger::error("Count is 0.");
                continue;
            }
            source.AddInitialItem(temp_formid, temp_count);
        }
        return source;
    }

    std::vector<Source> LoadYAMLSources(){
        logger::info("Loading yaml settings: Sources");
        std::vector<Source> sources;
        const auto folder_path = std::format("Data/SKSE/Plugins/{}", Utilities::mod_name) + "/presets";
        std::filesystem::create_directories(folder_path);
        logger::trace("Custom path: {}", folder_path);
        for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yml") {
                const auto filename = entry.path().string();
                YAML::Node config = YAML::LoadFile(filename);

                if (!config["containers"]) {
                    logger::trace("OwnerLists not found in {}", filename);
                    continue;
                }

                for (const auto& _Node : config["containers"]) {
                    // we have list of owners at each node or a scalar owner
                    const auto source = _parseSource(_Node);
                    if (source.formid == 0 && source.editorid.empty()) {
                        logger::error("LoadYAMLSources: File {} has invalid source: {}, {}", filename, source.formid, source.editorid);
						continue;
					}
                    sources.push_back(source);
                }
            }
        }
        return sources;
    };

    std::vector<Source> LoadSources(){
        std::vector<Source> sources;
        const auto IniSources = LoadINISources();
        logger::trace("IniSources size: {}", IniSources.size());
        sources.insert(sources.end(), IniSources.begin(), IniSources.end());
        const auto YamlSources = LoadYAMLSources();
        logger::trace("YamlSources size: {}", YamlSources.size());
        sources.insert(sources.end(), YamlSources.begin(), YamlSources.end());
        return sources;
    };

    const std::unordered_map<std::string,bool> LoadOtherSettings() {
    
        logger::info("Loading ini settings: OtherStuff");

        std::unordered_map<std::string, bool> others;

        CSimpleIniA ini;
        CSimpleIniA::TNamesDepend otherkeys;

        ini.SetUnicode();
        ini.LoadFile(path);


        // other stuff section
        bool val1 = ini.GetBoolValue(InISections[2], otherstuffKeys[0]);
        bool val2 = ini.GetBoolValue(InISections[2], otherstuffKeys[1]);
        bool val3 = ini.GetBoolValue(InISections[2], otherstuffKeys[2]);
        bool val4 = ini.GetBoolValue(InISections[2], otherstuffKeys[3]);
        others[otherstuffKeys[0]] = val1;
        others[otherstuffKeys[1]] = val2;
        others[otherstuffKeys[2]] = val3;
        others[otherstuffKeys[3]] = val4;

        // log the values
        logger::info("INI_changed_msg: {}", val1);
        logger::info("RemoveCarryBoosts: {}", val2);
        logger::info("ReturnToInitialMenu: {}", val3);
        logger::info("BatchSell: {}", val4);

        return others;
    }

    const std::unordered_set<std::string> AllowedFormTypes{ 
        "SCRL",  //	17 SCRL	ScrollItem
        "ARMO",  //	1A ARMO	TESObjectARMO
        "BOOK",  //	1B BOOK	TESObjectBOOK
        "INGR",  //	1E INGR	IngredientItem
        "MISC",  //	20 MISC TESObjectMISC
        "WEAP",  //	29 WEAP	TESObjectWEAP
        //"AMMO",  //	2A AMMO	TESAmmo
        "SLGM",  // 34 SLGM	TESSoulGem
        "ALCH",  //	2E ALCH	AlchemyItem
	};

    std::vector<int> xRemove = {
        0x99, 
        0x3C, 0x0B, 0x48,
        0x21,
        //
        // 0x24,
        0x70, 0x7E, 0x88, 0x8C, 0x1C};

};

