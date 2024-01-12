#pragma once

#include "Settings.h"



#include <iostream>

class Manager {

    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
    RE::NiPoint3 unownedChestPos = {1989.0740f, 1793.2015f, 6784.0000f};
    RE::TESObjectREFR* unownedChestObjRef;
    uint32_t total_chests = 1;

    void InitFailed() {
		logger::error("Failed to initialize Manager.");
		Utilities::MsgBoxesNotifs::InGame::InitErr();
		sources.clear();
		return;
	}

    void Init() {
        bool init_failed = false;
        for (auto& src : sources) {
            if (!src.GetFormByID(src.formid, src.editorid) 
                || !src.GetBoundObject()) {
                init_failed = true;
                continue;
            }
        }

        auto runtimeData = unownedCell->GetRuntimeData();
        if (!unownedCell || !unownedChest || !unownedChest->As<RE::TESBoundObject>()) {
            logger::error("Failed to initialize Manager due to missing unowned chest/cell");
			init_failed = true;
		}


        for (const auto& ref : runtimeData.references) {
            logger::info("Girdik");
            if (!ref) logger::info("Null object in cell");
            else {
                if (IsChest(ref->GetBaseObject()->GetFormID())) {
                    logger::info("Chest ref found in cell");
                    auto chest_ref_count = unownedChest->GetRefCount();
                    logger::info("Chest ref count {}", chest_ref_count);
                    unownedChestObjRef = ref.get();
                }
                logger::info("Object in cell: {}", ref->GetBaseObject()->GetName());
                logger::info("Object in cell: {}", ref->GetBaseObject()->GetFormEditorID());
            }
        }

        if (!unownedChestObjRef) {
			logger::info("Chest ref not found in cell. Adding...");
			auto newPropRef = MakeChest();
			unownedChestObjRef = newPropRef;
		}
        
        if (init_failed) InitFailed();

        logger::info("Manager initialized.");
    }

public:
    Manager(std::vector<Source>& data) : sources(data) { Init(); };

    static Manager* GetSingleton(std::vector<Source>& data) {
        static Manager singleton(data);
        return &singleton;
    }

    std::vector<Source> sources;

    bool RefIsContainer(RE::TESObjectREFR* ref) {
		if (!ref) return false;
		auto base = ref->GetBaseObject();
		if (!base) return false;
        auto formid = base->GetFormID();
        for (const auto& src : sources) {
			if (src.formid == formid) return true;
		}
        return false;
	}

    RE::TESObjectREFR* GetContainerChest(RE::TESObjectREFR* container) {
        for (const auto& src : sources) {
            if (src.paired_refid.find({ container->GetBaseObject()->GetFormID(), container->GetFormID() }) != src.paired_refid.end()) {
				auto chest_refid = src.paired_refid[{container->GetBaseObject()->GetFormID(), container->GetFormID()}];
				auto chest_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
				if (chest_ref) return chest_ref;
			}
        }
	};

    void ActivateChest(RE::TESObjectREFR* chestRef){
        unownedChest->Activate(chestRef, RE::PlayerCharacter::GetSingleton(), 0, unownedChest, 1);
    };

    bool IsChest(RE::FormID formid){
        return formid == unownedChest->GetFormID();
	};

    RE::TESObjectREFR* AddChest() {
        total_chests += 1;
        /*int total_chests_x = (((total_chests - 1) + 2) % 5) - 2;
        int total_chests_y = ((total_chests - 1) / 5) % 9;
        int total_chests_z = (total_chests - 1) / 45;*/
        int total_chests_x = (1 - (total_chests % 3)) * (-2);
        int total_chests_y = ((total_chests - 1) / 3) % 9;
        int total_chests_z = (total_chests - 1) / 27;
        float Pos3_x = unownedChestPos.x + 100.0f * total_chests_x;
        float Pos3_y = unownedChestPos.y + 50.0f * total_chests_y;
        float Pos3_z = unownedChestPos.z + 50.0f * total_chests_z;
        RE::NiPoint3 Pos3 = {Pos3_x, Pos3_y, Pos3_z};
        return MakeChest(Pos3);
    };

    RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) {
        auto item = unownedChest->As<RE::TESBoundObject>();
        
        auto newPropRef = RE::TESDataHandler::GetSingleton()
                ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f},
                                            unownedCell, nullptr, nullptr,
                                                          nullptr, {}, true, false).get().get();
        logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                     RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
        
        return newPropRef;
    };
    
    void MsgBoxCallback(unsigned int result, RE::TESObjectREFR* a_obj) { 
        logger::info("Result: {}", result); 
        /*if (result == 0) {
            if ()
			ActivateChest(unownedChestObjRef);
		}*/
    };

    void ActivateContainer(RE::TESObjectREFR* a_container) {
        std::vector<std::string> buttons = {"Open", "Carry", "Cancel"};
		unsigned int messageBoxId = 1;
        Utilities::MsgBoxesNotifs::ShowMessageBox(
            "asd", buttons, [this, a_container](unsigned int result) { this->MsgBoxCallback(result, a_container); });
	};


};
