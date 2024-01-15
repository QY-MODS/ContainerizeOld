#pragma once

#include "Settings.h"



#include <iostream>
#include <unordered_set>
//
//class Manager {
//    
//    // private variables
//    std::vector<std::string> buttons = {"Open", "Activate", "Cancel"};
//    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
//    RE::TESObjectREFR* current_container = nullptr;
//
//    // unowned stuff
//    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
//    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
//    RE::TESObjectREFR* unownedChestObjRef;
//    RE::NiPoint3 unownedChestPos = {1989.0740f, 1793.2015f, 6784.0000f};
//
//
//    // private functions
//    RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) {
//        auto item = unownedChest->As<RE::TESBoundObject>();
//
//        auto newPropRef = RE::TESDataHandler::GetSingleton()
//                              ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, unownedCell, nullptr, nullptr,
//                                                          nullptr, {}, true, false).get().get();
//        logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
//                     RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
//
//        return newPropRef;
//    };
//
//    Source* GetContainerSource(const RE::TESObjectREFR* a_container) {
//        logger::info("Getting container source");
//        auto container_formid = a_container->GetBaseObject()->GetFormID();
//        for (auto& src : sources) {
//            if (src.formid == container_formid) return &src;
//		}
//		return nullptr;
//    };
//
//
//    ItemListData GetSavedItems(RE::TESObjectREFR* a_container) {
//        logger::info("Getting saved items");
//        const auto src = GetContainerSource(a_container);
//        auto it = src->data.find(Utilities::GetEditorNameID(a_container));
//        if (it != src->data.end()) {
//        	logger::info("Saved items found");
//            if (it->second.size()) {
//                //logger::info("Saved items size: {}", it->second.size());
//				return it->second;
//            }
//        }
//        return ItemListData();
//    };
//
//    //void UpdateItemListData(RE::TESObjectREFR* container, RE::TESObjectREFR::InventoryItemMap inventory_map) {
//    //    if (!container) return;
//    //    logger::info("Updating item list data");
//    //    auto src = GetContainerSource(container);
//    //    src->data[Utilities::GetEditorNameID(container)] = inventory_map;//ConvertToItemListData(inventory_map);
//    //    logger::info("Item list data updated");
//    //    src->PrintSourceEditorRefIDs();
//    //};
//
//
//    void RemoveAllItemsFromChest(bool _updateItemListData = false ) { 
//        auto items = unownedChestObjRef->GetInventoryCounts();
//        logger::info("Removing {} items from chest", items.size());
//
//        // Save the content of the chest
//    //    if (_updateItemListData) {
//    //        if (!current_container) logger::info("Current container is null!!!! How???");
//    //        else {
//    //            auto src = GetContainerSource(current_container);
//    //            ItemListData myitems;
//    //            auto inventory = unownedChestObjRef->GetInventory();
//    //            for (const auto& item : inventory) {
//    //                auto extralist = (*item.second.second).extraLists;
//    //                RE::ExtraDataList myextra;
//    //                if (!extralist) logger::info("extralist is null");
//    //                myextra = *extralist->front();
//    //                EditorID myeditorid =
//    //                    Utilities::GetEditorID(RE::TESForm::LookupByID<RE::TESForm>(item.first->GetFormID()));
//    //                ItemData myitem(item.second.first, myextra, myeditorid);
//    //                myitems.push_back(myitem);
//				//}
//    //            src->data[Utilities::GetEditorNameID(current_container)] = myitems;
//    //            logger::info("Item list data updated. Printing source...");
//    //            src->PrintSourceEditorRefIDs();
//
//    //            auto extras = unownedChestObjRef->extraList; // <- bunu da dene bi
//    //            //UpdateItemListData(current_container, unownedChestObjRef->GetInventory());
//    //        }
//    //    }
//
//        for (const auto& item : items) {
//            unownedChestObjRef->RemoveItem(item.first, item.second, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
//		}
//	};
//
//
//    void AddItems2Chest(const ItemListData& items) {
//        ItemData temp;
//        for (auto& it : items) {
//            logger::info("ölkä");
//            // add item to chest
//            //temp = it;
//            if (!Utilities::AddItemToRef(unownedChestObjRef, it)) {
//                logger::info("NOPE");
//                logger::info("Could not find item with editorid {}", it.editorid);
//                Utilities::MsgBoxesNotifs::InGame::ProblemWithItem(it.editorid);
//                return;
//            }
//            logger::info("ölkä2");
//        }
//    };
//
//
//    void Activate(RE::TESObjectREFR* a_objref) {
//        if (!a_objref) {
//        	logger::info("a_objref is null");
//			return;
//        }
//        auto player = RE::PlayerCharacter::GetSingleton();
//        auto a_obj = a_objref->GetBaseObject()->As<RE::TESObjectCONT>();
//        if (!a_obj) return;
//        logger::info("ref count {}", a_obj->GetRefCount());
//        a_obj->Activate(a_objref, player, 0, a_obj, 1);
//    }
//
//    void ActivateChest(const char* container_name) {
//        unownedChest->fullName = container_name;
//        Activate(unownedChestObjRef);
//        
//    };
//    
// //   void AddItems2Player(const ItemListData itemlist) {
//	//	for (const auto& item : itemlist) {
//	//		const Utilities::Types::EditorID& editorID = item.first;
//	//		const unsigned int n_item = item.second;
//	//		auto item_form = RE::TESForm::LookupByEditorID<RE::TESBoundObject>(editorID);
//	//		if (!item_form) {
// //               logger::info("Could not find item with editorid {}", editorID);
//	//			Utilities::MsgBoxesNotifs::InGame::ProblemWithItem(editorID);
//	//			continue;
//	//		}
//	//		player_ref->AddObjectToContainer(item_form, nullptr, n_item, nullptr);
//	//	}
//	//};
//
//    void MsgBoxCallback(unsigned int result) {
//        logger::info("Result: {}", result);
//
//        if (result != 0 && result != 1 && result != 2) return;
//
//        // Cancel
//        if (result == 2) return;
//
//        // cancellamadiimiz icin her turlu gerekiyo
//        // Add saved items to the unowned chest
//        logger::info("Getting saved items");
//        ItemListData items = GetSavedItems(current_container);
//        logger::info("Adding items to chest");
//        AddItems2Chest(items);
//
//        // Activating container (also transfering all items to player)
//        if (result == 1) {
//            listen_activate = false; 
//            current_container->SetActivationBlocked(0);
//            logger::info("display name: {}", current_container->GetDisplayFullName());
//            current_container->GetBaseObject()->Activate(current_container, player_ref, 1, nullptr, 1);
//            logger::info("display name: {}", current_container->GetDisplayFullName());
//            current_container->SetActivationBlocked(1);
//            listen_activate = true;
//            return;
//        }
//
//        // Opening container
//
//        // Listen for menu close
//        listen_menuclose = true;
//
//        // Activate the unowned chest
//        logger::info("Activating chest");
//        ActivateChest(current_container->GetName());
//        logger::info("Chest activated");
//
//    };
//
//    void InitFailed() {
//		logger::info("Failed to initialize Manager.");
//		Utilities::MsgBoxesNotifs::InGame::InitErr();
//		sources.clear();
//		return;
//	}
//
//    void Init() {
//        bool init_failed = false;
//
//        if (!sources.size()) {
//			logger::info("No sources found.");
//            InitFailed();
//            return;
//        }
//
//
//        for (auto& src : sources) {
//            if (!Utilities::GetFormByID(src.formid, src.editorid) 
//                || !src.GetBoundObject()) {
//                init_failed = true;
//                logger::info("Failed to initialize Manager due to missing source");
//                break;
//            }
//        }
//
//        std::unordered_set<std::uint32_t> encounteredFormIDs;
//
//        for (const auto& source : sources) {
//            // Check if the formid is already encountered
//            if (!encounteredFormIDs.insert(source.formid).second) {
//                // Duplicate formid found
//                logger::info("Duplicate formid found: {}", source.formid);
//                init_failed = true;
//            }
//        }
//
//
//        // Check if unowned chest is in the cell
//        auto runtimeData = unownedCell->GetRuntimeData();
//        if (!unownedCell || !unownedChest || !unownedChest->As<RE::TESBoundObject>()) {
//            logger::info("Failed to initialize Manager due to missing unowned chest/cell");
//			init_failed = true;
//		}
//
//
//        for (const auto& ref : runtimeData.references) {
//            logger::info("Girdik");
//            if (!ref) logger::info("Null object in cell");
//            else if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
//                logger::info("Chest ref found in cell");
//                auto chest_ref_count = unownedChest->GetRefCount();
//                logger::info("Chest ref count {}", chest_ref_count);
//                unownedChestObjRef = ref.get();
//                RemoveAllItemsFromChest();
//                break;
//            }
//        }
//
//        if (!unownedChestObjRef) {
//            init_failed = true;
//			/*logger::info("Chest ref not found in cell. Adding...");
//			auto newPropRef = MakeChest();
//			unownedChestObjRef = newPropRef;*/
//		}
//        
//        if (init_failed) InitFailed();
//
//        logger::info("Manager initialized.");
//    }
//
//public:
//    Manager(std::vector<Source>& data) : sources(data) { Init(); };
//
//    static Manager* GetSingleton(std::vector<Source>& data) {
//        static Manager singleton(data);
//        return &singleton;
//    }
//
//    std::vector<Source> sources;
//    bool listen_menuclose = false;
//    bool listen_activate = true;
//
//    bool RefIsContainer(RE::TESObjectREFR* ref) {
//		if (!ref) return false;
//		auto base = ref->GetBaseObject();
//		if (!base) return false;
//        auto formid = base->GetFormID();
//        for (const auto& src : sources) {
//			if (src.formid == formid) return true;
//		}
//        return false;
//	}
//
//
//    void ActivateContainer(RE::TESObjectREFR* a_container) {
//
//        auto src = GetContainerSource(a_container);
//        if (!src) {
//            logger::info("Could not find source for container");
//			return;
//		}
//        
//        std::string name = a_container->GetDisplayFullName();
//        if (!name.ends_with(Settings::suffix) &&
//            Utilities::EqStr(a_container->GetDisplayFullName(), a_container->GetName())) {
//            logger::info("Adding suffix to new container with display name: {} real name: {}", name,a_container->GetName());
//            if (!src->nameIDs.insert(name).second) {
//                name = Utilities::generateUniqueName(name, src->nameIDs);
//                if (!src->nameIDs.insert(name).second) {
//					logger::info("Could not generate unique name for container!!!Could not generate unique name for container!!!");
//				}
//            }
//			a_container->SetDisplayName(name+Settings::suffix, 1);
//        } else
//            logger::info("Container already opened: {} real name: {})", name, a_container->GetName());
//        
//
//        name = a_container->GetDisplayFullName();
//        current_container = a_container;
//        Utilities::MsgBoxesNotifs::ShowMessageBox(
//            name + " " + std::to_string(a_container->GetBaseObject()->GetFormID()),
//            buttons,
//            [this](unsigned int result) { this->MsgBoxCallback(result); });
//    };
//
//    void DeactivateContainer() {
//		if (!current_container) return;
//        RemoveAllItemsFromChest(true);
//    }
//
//};
