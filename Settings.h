#pragma once
#include "Utils.h"




using namespace Utilities::Types;

struct Source {

    float capacity;
    std::uint32_t formid;
    std::string editorid;
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
        auto form = Utilities::FunctionsSkyrim::GetFormByID(formid, editorid);
        if (form)
            return form->GetName();
        else
            return "";
    };

    RE::TESBoundObject* GetBoundObject() {
        return Utilities::FunctionsSkyrim::GetFormByID<RE::TESBoundObject>(formid, editorid);
    };
};


namespace Settings {


    constexpr auto path = L"Data/SKSE/Plugins/Containerize.ini";


    bool po3installed = false;

    constexpr std::array<const char*, 3> InISections = {"Containers",
                                                        "Capacities",
                                                        "Other Stuff"};
    constexpr std::array<const char*, 2> InIDefaultKeys = {"src1", "src1"};
    constexpr std::array<const char*, 2> InIDefaultVals = {"", ""};

    const std::array<std::string, 3> section_comments =
        {";Make sure to use unique keys, e.g. src1=... NOTsrc1=...",
         std::format(";Make sure to use matching keys with the ones provided in section {}.",
                     static_cast<std::string>(InISections[0])),
        ";Other gameplay related settings",
        };


    const std::array<std::string, 4> os_comments =
		{";Set to false to suppress the 'INI changed between saves' message.",
		"; Set to true to remove the initial carry weight bonuses on your container items.",
        "; Set to true to return to the the initial menu after closing your container's menu (which you had opened by holding equip).",
        "; Set to true to sell your container to vendors together with the items inside it.",
		};

   
    constexpr std::uint32_t kSerializationVersion = 729;
    constexpr std::uint32_t kDataKey = 'CTRZ';

    constexpr std::array<const char*, 4> otherstuffKeys = 
    {"INI_changed_msg", "RemoveCarryBoosts","ReturnToInitialMenu", "BatchSell"};
    constexpr std::array<bool, 4> otherstuffVals = {true, true, true, true};

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
            ini.SetBoolValue(InISections[2], otherstuffKeys[0], true, os_comments[0].c_str());
            ini.SetBoolValue(InISections[2], otherstuffKeys[1], false, os_comments[1].c_str());
            ini.SetBoolValue(InISections[2], otherstuffKeys[2], false, os_comments[2].c_str());
            logger::info("Default values set for section {}", InISections[2]);
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

                // if both formid is valid hex, use it
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

    const std::unordered_map<std::string,bool> LoadOtherSettings() {
    
        logger::info("Loading ini settings: OtherStuff");

        std::unordered_map<std::string, bool> others;

        CSimpleIniA ini;
        CSimpleIniA::TNamesDepend otherkeys;

        ini.SetUnicode();
        ini.LoadFile(path);

        // Create Sections with defaults if they don't exist
        if (!ini.SectionExists(InISections[2])) {
            ini.SetBoolValue(InISections[2], InIDefaultKeys[2], true, section_comments[2].c_str());
            logger::info("Default values set for section {}", InISections[2]);
        }


        ini.GetAllKeys(InISections[2], otherkeys);
        auto numOthers = otherkeys.size();
        logger::info("otherkeys size {}", numOthers);

        if (numOthers == 0 || numOthers != otherstuffKeys.size()) {
			logger::warn("No other settings found in the ini file or Invalid number of other settings . Using defaults.");
            ini.SetBoolValue(InISections[2], otherstuffKeys[0], otherstuffVals[0]);
            ini.SetBoolValue(InISections[2], otherstuffKeys[1], otherstuffVals[1]);
            ini.SetBoolValue(InISections[2], otherstuffKeys[2], otherstuffVals[2]);
            ini.SetBoolValue(InISections[2], otherstuffKeys[3], otherstuffVals[3]);
        }

        bool val1;
        bool val2;
        bool val3;
        bool val4;
        // other stuff section
        val1 = ini.GetBoolValue(InISections[2], otherstuffKeys[0]);
        ini.SetBoolValue(InISections[2], otherstuffKeys[0], val1, os_comments[0].c_str());
        val2 = ini.GetBoolValue(InISections[2], otherstuffKeys[1]);
        ini.SetBoolValue(InISections[2], otherstuffKeys[1], val2, os_comments[1].c_str());
        val3 = ini.GetBoolValue(InISections[2], otherstuffKeys[2]);
        ini.SetBoolValue(InISections[2], otherstuffKeys[2], val3, os_comments[2].c_str());
        val4 = ini.GetBoolValue(InISections[2], otherstuffKeys[3]);
		ini.SetBoolValue(InISections[2], otherstuffKeys[3], val4, os_comments[3].c_str());
        others[otherstuffKeys[0]] = val1;
        others[otherstuffKeys[1]] = val2;
        others[otherstuffKeys[2]] = val3;
        others[otherstuffKeys[3]] = val4;

        ini.SaveFile(path);

        return others;
    }

    const std::unordered_set<std::string> AllowedFormTypes{ 
        "SCRL",  //	17 SCRL	ScrollItem
        "ARMO",  //	1A ARMO	TESObjectARMO
        "BOOK",  //	1B BOOK	TESObjectBOOK
        "INGR",  //	1E INGR	IngredientItem
        "MISC",  //	20 MISC TESObjectMISC
        "WEAP",  //	29 WEAP	TESObjectWEAP
        "AMMO",  //	2A AMMO	TESAmmo
        "SLGM",  // 34 SLGM	TESSoulGem
        "ALCH",  //	2E ALCH	AlchemyItem
	};


    //enum class AllowedFormTypes {
    //    "SCRL",  //	17 SCRL	ScrollItem
    //    Armor,   //	1A ARMO	TESObjectARMO
    //    Book,    //	1B BOOK	TESObjectBOOK
    //    // Container,                   //	1C CONT	TESObjectCONT
    //    // Door,                        //	1D DOOR	TESObjectDOOR
    //    Ingredient,  //	1E INGR	IngredientItem
    //    Misc,        //	20 MISC TESObjectMISC
    //    // Apparatus,                   //	21 APPA	BGSApparatus
    //    // Static,                      //	22 STAT	TESObjectSTAT
    //    // StaticCollection,            //	23 SCOL BGSStaticCollection
    //    // MovableStatic,               //	24 MSTT	BGSMovableStatic
    //    // Furniture,                   //	28 FURN	TESFurniture
    //    Weapon,  //	29 WEAP	TESObjectWEAP
    //    Ammo,    //	2A AMMO	TESAmmo
    //    // NPC,                         //	2B NPC_	TESNPC
    //    // LeveledNPC,                  //	2C LVLN	TESLevCharacter
    //    // KeyMaster,                   //	2D KEYM	TESKey
    //    // AlchemyItem,                 //	2E ALCH	AlchemyItem
    //    // Note,                        //	30 NOTE	BGSNote
    //    // ConstructibleObject,         //	31 COBJ	BGSConstructibleObject
    //    SoulGem,  //	34 SLGM	TESSoulGem
    //    // LeveledItem,                 //	35 LVLI	TESLevItem
    //    // LeveledSpell,                //	52 LVSP	TESLevSpell
    //    // AnimatedObject,              //	53 ANIO	TESObjectANIO
    //};

};

