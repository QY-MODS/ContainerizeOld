#pragma once
#include "Utils.h"




using namespace Utilities::Types;

struct Source {

    const float capacity;
    std::uint32_t formid;
    const std::string editorid;
    SourceData data;

    Source(std::uint32_t id, const std::string id_str, float capacity)
        : formid(id), editorid(id_str), capacity(capacity) {
        if (!formid) {
            auto form = RE::TESForm::LookupByEditorID<RE::TESForm>(editorid);
            if (form) {
                logger::info("Found formid for editorid {}", editorid);
                formid = form->GetFormID();
            } else
                logger::info("Could not find formid for editorid {}", editorid);
        }
    };
    
    std::string_view GetName() {
        auto form = Utilities::GetFormByID(formid, editorid);
        if (form)
            return form->GetName();
        else
            return "";
    };

    RE::TESBoundObject* GetBoundObject() { return Utilities::GetFormByID<RE::TESBoundObject>(formid, editorid); };

    void RemoveDataEntry(RefID refid) {
		auto it = data.find(refid);
		if (it != data.end()) {
			data.erase(it);
			logger::info("Removed data entry for refid {}", refid);
        } else {
			logger::info("Could not find data entry for refid {}", refid);
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
        }
	};
};


namespace Settings {

    // Assuming Utilities::mod_name is of type std::string

    constexpr auto path = L"Data/SKSE/Plugins/EverythingContainer.ini";

    //const std::string suffix = " | EC";
    //const std::string suffix = "";


    bool po3installed = false;

    constexpr std::array<const char*, 3> InISections = {"Containers",
                                                        "Capacities",
                                                        "Other Stuff"};
    constexpr std::array<const char*, 2> InIDefaultKeys = {"src1", "src1"};

    const std::array<std::string, 3> section_comments =
        {";Make sure to use unique keys, e.g. src1=... NOTsrc1=...",
         std::format(";Make sure to use matching keys with the ones provided in section {}.",
                     static_cast<std::string>(InISections[0])),
        //";Set boolean values in this section, i.e. true or false."
        };

    //bool force_editor_id = false;
    //bool force_editor_id = true;

    // DO NOT CHANGE THE ORDER OF THESE
    /*constexpr std::array<const char*, 1> OtherStuffDefKeys = {"ForceEditorID"};
    const std::map<const char*, std::map<bool, const char*>> other_stuff_defaults = {
        {OtherStuffDefKeys[0],
         {{force_editor_id,
           ";Set to true if you ONLY use EditorIDs and NO FormIDs AND you have powerofthree's Tweaks installed, "
           "otherwise false."}}}};*/


    constexpr std::uint32_t kSerializationVersion = 123;
    constexpr std::uint32_t kDataKey = 'CONT';


    std::vector<Source> LoadINISettings() {
        
        logger::info("Loading ini settings");

        std::vector<Source> sources;

        // Check if powerofthree's Tweaks is installed
        if (Utilities::IsPo3Installed()) {
            logger::info("powerofthree's Tweaks is installed. Enabling EditorID support.");
            po3installed = true;
        } else {
            logger::info("powerofthree's Tweaks is not installed. Disabling EditorID support.");
            po3installed = false;
        }

        // if po3 is mandatory but not installed, return
   //     if (!po3installed) {
   //         Utilities::MsgBoxesNotifs::Windows::Po3ErrMsg();
			//return sources;
   //     }


        //// Open the file in output mode, creating it if it doesn't exist
        //if (!std::filesystem::exists(path)) {
        //    logger::info("File does not exist. Creating it.");
        //    std::wofstream outputFile(path);

        //    // Check if the file is open
        //    if (!outputFile.is_open()) {
        //        logger::info("Error: Could not open the file for writing.");
        //        return sources;
        //    };

        //    // Close the file
        //    outputFile.close();

        //    Utilities::MsgBoxesNotifs::InGame::IniCreated();

        //}


        CSimpleIniA ini;
        CSimpleIniA::TNamesDepend source_names;

        ini.SetUnicode();
        ini.LoadFile(path);

        // Create Sections with defaults if they don't exist
        for (int i = 0; i < InISections.size(); ++i) {
            logger::info("Checking section {}", InISections[i]);
            if (!ini.SectionExists(InISections[i])) {
                logger::info("Section {} does not exist. Creating it.", InISections[i]);
                ini.SetValue(InISections[i], nullptr, nullptr);
                logger::info("Setting default keys for section {}", InISections[i]);
                if (i < InISections.size() - 1) {
                    ini.SetValue(InISections[i], InIDefaultKeys[i], nullptr, section_comments[i].c_str());
                    logger::info("Default values set for section {}", InISections[i]);
                } /*else {
                    logger::info("Creating Other Stuff section");
                    for (auto it = other_stuff_defaults.begin(); it != other_stuff_defaults.end(); ++it) {
                        auto it2 = it->second.begin();
                        ini.SetBoolValue(InISections[i], it->first, it2->first, it2->second);
                    }
                }*/
            }
        }

        // Sections: Other stuff
        // get from user
        /*CSimpleIniA::TNamesDepend other_stuff_userkeys;
        ini.GetAllKeys(InISections[InISections.size()-1], other_stuff_userkeys);
        for (CSimpleIniA::TNamesDepend::const_iterator it = other_stuff_userkeys.begin();
             it != other_stuff_userkeys.end(); ++it) {
            for (auto it2 = other_stuff_defaults.begin(); it2 != other_stuff_defaults.end(); ++it2) {
                if (static_cast<std::string_view>(it->pItem) == static_cast<std::string_view>(it2->first)) {
                    logger::info("Found key: {}", it->pItem);
                    auto it3 = it2->second.begin();
                    auto val = ini.GetBoolValue(InISections[InISections.size() - 1], it->pItem, it3->first);
                    ini.SetBoolValue(InISections[InISections.size() - 1], it->pItem, val, it3->second);
                    break;
                }
            }
        }*/

        //// set stuff which is not found
        //for (auto it = other_stuff_defaults.begin(); it != other_stuff_defaults.end(); ++it) {
        //    if (ini.KeyExists(InISections[InISections.size() - 1], it->first)) continue;
        //    auto it2 = it->second.begin();
        //    ini.SetBoolValue(InISections[InISections.size() - 1], it->first, it2->first, it2->second);
        //}

        //force_editor_id = ini.GetBoolValue(InISections[InISections.size() - 1],
        //                     OtherStuffDefKeys[0]);  // logger::info("force_editor_id: {}", force_editor_id);

        // Sections: Containers, Capacities
        ini.GetAllKeys(InISections[0], source_names);
        auto numSources = source_names.size();
        logger::info("source_names size {}", numSources);

        sources.reserve(numSources);
        const char* val1;
        const char* val2;
        uint32_t id;
        std::string id_str;

        for (CSimpleIniA::TNamesDepend::const_iterator it = source_names.begin(); it != source_names.end(); ++it) {
            logger::info("source name {}", it->pItem);
            val1 = ini.GetValue(InISections[0], it->pItem);
            val2 = ini.GetValue(InISections[1], it->pItem);
            if (!val1 || !val2 || !std::strlen(val1) || !std::strlen(val2)) {
                logger::warn("Source {} is missing a value. Skipping.", it->pItem);
                continue;
            } 
            else {
                logger::info("Source {} has a value of {}", it->pItem, val1);
                logger::info("We have valid entries for container: {} and capacity: {}", val1, val2);
                // back to container_id and capacity
                id = static_cast<uint32_t>(std::strtoul(val1, nullptr, 16));
                id_str = static_cast<std::string>(val1);
                // Our job is easy if the user wants to force editor ids
                //if (force_editor_id && po3installed) sources.emplace_back(0, id_str, std::stof(val2));
                // if both formids are valid hex, use them
                if (Utilities::isValidHexWithLength7or8(val1))
                    sources.emplace_back(id, "", std::stof(val2));
                else if (!po3installed) {
                    Utilities::MsgBoxesNotifs::Windows::Po3ErrMsg();
                    return sources;
                } else
                    sources.emplace_back(0, id_str, std::stof(val2));

                ini.SetValue(InISections[0], it->pItem, val1);
                ini.SetValue(InISections[1], it->pItem, val2);
                logger::info("Loaded container: {} with capacity: {}", val1, std::stof(val2));
            }
        }

        ini.SaveFile(path);


        return sources;
    }

};

