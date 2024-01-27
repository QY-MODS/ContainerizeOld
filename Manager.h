#pragma once

#include "Settings.h"


class Manager : public Utilities::BaseFormRefIDRefID {
    // private variables

    bool isUninstalled = false;


    std::vector<std::string> buttons = {"Open", "Activate", "Cancel", "More..."};
    std::vector<std::string> buttons_more = {"Uninstall", "Back", "Cancel"};
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    RE::TESObjectREFR* current_container = nullptr;

    // unowned stuff
    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
    // RE::TESObjectREFR* unownedChestObjRef;
    RE::NiPoint3 unownedChestPos = {1989.0740f, 1793.2015f, 6784.0000f};

    // private functions
    RE::TESObjectREFR* MakeChest(RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) {
        auto item = unownedChest->As<RE::TESBoundObject>();

        auto newPropRef = RE::TESDataHandler::GetSingleton()
                              ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, unownedCell, nullptr, nullptr,
                                                          nullptr, {}, true, false).get().get();
        logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                     RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());

        return newPropRef;
    };

    RE::TESObjectREFR* AddChest(const uint32_t chest_no) {
        auto total_chests = chest_no;
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

    RE::TESObjectREFR* FindNotMatchedChest() {
        auto runtimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : runtimeData.references) {
            if (!ref) continue;
            if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
                size_t no_items = ref->GetInventory().size();
                if (!no_items && !Utilities::ValueExists<SourceDataKey>(GetAllData(),ref->GetFormID())) return ref.get();
            }
        }
        return AddChest(GetNoChests());
    };

    uint32_t GetNoChests() {
        auto runtimeData = unownedCell->GetRuntimeData();
        uint32_t no_chests = 0;
        for (const auto& ref : runtimeData.references) {
            if (!ref) continue;
            if (ref->IsDeleted()) continue;
            if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
                no_chests++;
            }
        }
        return no_chests;
    };

    RE::TESObjectREFR* GetContainerChest(RE::TESObjectREFR* container){
        if (!container) {
            logger::info("container reference is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return nullptr;
        }

        auto src = GetContainerSource(container->GetBaseObject()->GetFormID());
        if (!src) {
            logger::info("Could not find source for container");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return nullptr;
        }

        auto container_refid = container->GetFormID();
        if (src->data.find(container_refid) == src->data.end()) {
            logger::info("Container refid not found in source");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return nullptr;
        }

        auto chest_refid = src->data[container_refid];
        logger::info("Chest refid: {}", chest_refid);

        if (!chest_refid) {
            logger::info("Chest refid is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return nullptr;
        }

        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chest) {
            logger::info("Could not find chest");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return nullptr;
        }
        return chest;
    }

    Source* GetContainerSource(const FormID container_formid) {
        logger::info("Getting container source");
        for (auto& src : sources) {
            if (src.formid == container_formid) return &src;
        }
        logger::info("Container source not found");
        return nullptr;
    };

    void RemoveAllItemsFromChest(RE::TESObjectREFR* chest, bool transfer2player = true) {

        auto move2ref = transfer2player ? player_ref : nullptr;

        auto chest_container = chest->GetContainer();
        if (!chest_container) {
            logger::error("Chest container is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return;
        }

        //// Lambda function to remove each item from the chest
        // auto removeItemLambda = [&chest_container, &move2ref, &chest](RE::ContainerObject& obj) {
        //     // Assuming that RemoveObjectFromContainer returns true on success
        //     auto item_count = chest_container->numContainerObjects;
        //     logger::info("Item count: {}", item_count);
        //     chest->RemoveItem(obj.obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, {obj.itemExtra},
        //                                                  move2ref);
        //     //    // Optionally, you can transfer the removed item to the player
        //     //    chest_container
        //     //    if (move2ref) {
        //     //        move2ref->GetContainer()->AddObjectToContainer(obj.obj, obj.count, nullptr);
        //     //    }
        //     //    logger::info("Removed {} {} from chest", obj.count, obj.obj->GetName());
        //     //    return RE::BSContainer::ForEachResult::kContinue;  // Continue the iteration
        //     //} else {
        //     //    // Handle removal failure
        //     //    logger::error("Failed to remove {} {} from chest", obj.count, obj.obj->GetName());
        //     //    return RE::BSContainer::ForEachResult::kStop;  // Stop the iteration
        //     //}
        // };

        // chest_container->ForEachContainerObject(removeItemLambda);

        auto inventory = chest->GetInventory();
        logger::info("Removing {} items from chest", inventory.size());
        for (const auto& item : inventory) {
            auto item_obj = item.first;
            auto item_count = item.second.first;
            logger::info("Item count: {}", item_count);
            logger::info("Item extradatalists");
            //         uint32_t item_extradatalists_size = 0;
            //         for (auto it_ = item.second.second->extraLists->begin(); it_ !=
            //         item.second.second->extraLists->end();
            //              ++it_) {
            //	item_extradatalists_size++;
            //}
            //         if (item_extradatalists_size != 1) {
            //         	logger::info("Item has more than one extra data list");
            //             Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            //         }

            logger::info("Removing item2");
            // RE::InventoryEntryData* derivedPtr = static_cast<RE::InventoryEntryData*>(item.second.second.get());
            // std::unique_ptr<int> newUniquePtr = std::move(item.second.second);
            auto inv_data = item.second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                logger::info("Extra list is null");
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, move2ref);
            } else {
                logger::info("Removing item2.5");
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, asd->front(), move2ref);
            }
            logger::info("Removing item3");
            /*for (auto it = asd->begin(); it != asd->end(); ++it) {
                if (!*it) continue;
                logger::info("1");
                auto extra = *it;
            }*/
            /*auto inv_data = item.second.second.get();
            logger::info("item display name: {}", inv_data->GetDisplayName());
            logger::info("item weight: {}", inv_data->GetWeight());*/
            // weight += inv_data->GetWeight();
            /*auto asd = inv_data->extraLists;
            if (!asd) continue;
            logger::info("Extra listttt");*/
        }
    };

    void Activate(RE::TESObjectREFR* a_objref) {
        if (!a_objref) {
            logger::info("Object reference is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return;
        }
        auto player = RE::PlayerCharacter::GetSingleton();
        auto a_obj = a_objref->GetBaseObject()->As<RE::TESObjectCONT>();
        if (!a_obj) return;
        a_obj->Activate(a_objref, player, 0, a_obj, 1);
    }

    void ActivateChest(RE::TESObjectREFR* chest, const char* chest_name) {
        unownedChest->fullName = chest_name;
        Activate(chest);
    };

    void MsgBoxCallback(unsigned int result) {
        logger::info("Result: {}", result);

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

        // More
        if (result == 3) {
            return Utilities::MsgBoxesNotifs::ShowMessageBox(
                "...", buttons_more, [this](unsigned int res) { this->MsgBoxCallbackMore(res); });
        }

        // Cancel
        if (result == 2) return;
        
        // Activating container (also transfering all items to player)
        if (result == 1) {
            listen_activate = false;
            current_container->SetActivationBlocked(0);
            const auto container_refid = current_container->GetFormID();
            auto chest = GetContainerChest(current_container);
            current_container->GetBaseObject()->Activate(current_container, player_ref, 1, nullptr, 1);
            if (!current_container->GetFormID()) {
                RemoveAllItemsFromChest(chest, true);
                auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
                auto refIt = src->data.find(container_refid);
                if (refIt != src->data.end()) {
                    // Remove the entry
                    src->data.erase(refIt);
                    logger::info("RefID {} removed from sources.", container_refid);
                } else {
                    logger::info("RefID {} not found in sources.", container_refid);
                    Utilities::MsgBoxesNotifs::InGame::GeneralErr();
                }

            } else
                current_container->SetActivationBlocked(1);
            listen_activate = true;

            return;
        }

        // Opening container

        // Listen for menu close
        listen_menuclose = true;

        // Activate the unowned chest
        logger::info("Activating chest");
        ActivateChest(GetContainerChest(current_container), current_container->GetName());
    };

    void MsgBoxCallbackMore(unsigned int result) {
        logger::info("More");

        if (result != 0 && result != 1 && result != 2) return;

        // Cancel
        if (result == 2) return;

        // Back
        if (result == 1) {
            auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
            std::string name = current_container->GetDisplayFullName();
            std::ostringstream stream1;
            stream1 << std::fixed << std::setprecision(2)
                    << GetContainerChest(current_container)->GetWeightInContainer();
            std::ostringstream stream2;
            stream2 << std::fixed << std::setprecision(2) << src->capacity;

            return Utilities::MsgBoxesNotifs::ShowMessageBox(
                name + ": " + stream1.str() + "/" + stream2.str(), buttons,
                [this](unsigned int result) { this->MsgBoxCallback(result); });
        }

        // Uninstall
        bool uninstall_successful = true;
        logger::info("Uninstalling...");
        logger::info("No of chests in cell: {}", GetNoChests());
        for (auto src = sources.begin(); src != sources.end(); ++src) {
            if (!uninstall_successful) break;
            for (auto it = src->data.begin(); it != src->data.end();) {
                // here i could call MsgBoxCallback(1), i.e. Activate. For now, I leave it decoupled.
                current_container = RE::TESForm::LookupByID<RE::TESObjectREFR>(it->first);
                current_container->SetActivationBlocked(0);
                auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(it->second);
                if (!chest) {
                    uninstall_successful = false;
                    logger::info("Chest with refid {} not found.", it->second);
                    break;
                } else {
                    RemoveAllItemsFromChest(chest, true);
                    if (chest->GetInventory().size()) {
                        uninstall_successful = false;
                        logger::info("Chest with refid {} is not empty.", it->second);
                        break;
                    }
                    chest->Disable();
                    chest->SetDelete(true);
                    it = src->data.erase(it);
                }
            }
            if (!src->data.empty()) {
                uninstall_successful = false;
                logger::info("Source with formid {} is not empty.", src->formid);
                break;
            }
        }
        logger::info("uninstall_successful: {}", uninstall_successful);

        logger::info("No of chests in cell: {}", GetNoChests());
        if (GetNoChests() != 1) uninstall_successful = false;
        logger::info("uninstall_successful: {}", uninstall_successful);

        if (uninstall_successful) {
            sources.clear();
            current_container = nullptr; 
            logger::info("Uninstall successful.");
            Utilities::MsgBoxesNotifs::InGame::UninstallSuccessful();
        } else {
            logger::info("Uninstall failed.");
            Utilities::MsgBoxesNotifs::InGame::UninstallFailed();
        }

        isUninstalled = true;
    }

    void InitFailed() {
        logger::error("Failed to initialize Manager.");
        Utilities::MsgBoxesNotifs::InGame::InitErr();
        sources.clear();
        isUninstalled = true;
        return;
    }

    void Init() {
        bool init_failed = false;

        if (!sources.size()) {
            logger::error("No sources found.");
            InitFailed();
            return;
        }

        for (auto& src : sources) {
            if (!Utilities::GetFormByID(src.formid, src.editorid) || !src.GetBoundObject()) {
                init_failed = true;
                logger::error("Failed to initialize Manager due to missing source: {}",src.formid);
                break;
            }
        }

        std::unordered_set<std::uint32_t> encounteredFormIDs;

        for (const auto& source : sources) {
            // Check if the formid is already encountered
            if (!encounteredFormIDs.insert(source.formid).second) {
                // Duplicate formid found
                logger::error("Duplicate formid found: {}", source.formid);
                init_failed = true;
            }
        }

        // Check if unowned chest is in the cell
        auto runtimeData = unownedCell->GetRuntimeData();
        if (!unownedCell || !unownedChest || !unownedChest->As<RE::TESBoundObject>()) {
            logger::error("Failed to initialize Manager due to missing unowned chest/cell");
            init_failed = true;
        }

        // for (const auto& ref : runtimeData.references) {
        //     logger::info("Girdik");
        //     if (!ref) logger::info("Null object in cell");
        //     else if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
        //         logger::info("Chest ref found in cell");
        //         auto chest_ref_count = unownedChest->GetRefCount();
        //         logger::info("Chest ref count {}", chest_ref_count);
        //         unownedChestObjRef = ref.get();
        //         RemoveAllItemsFromChest(false);
        //         break;
        //     }
        // }

        //      if (!unownedChestObjRef) {
        //          init_failed = true;
        //	/*logger::info("Chest ref not found in cell. Adding...");
        //	auto newPropRef = MakeChest();
        //	unownedChestObjRef = newPropRef;*/
        //}

        if (init_failed) InitFailed();

        logger::info("Manager initialized.");
    }

    SourceData GetAllData() {
        // Merged map to store the result
        std::map<RefID, RefID> mergedData;

        // Lambda function to merge the 'data' map of a Source into mergedData
        auto mergeData = [&mergedData](const Source& source) {
            for (const auto& entry : source.data) {
                // Insert the entry into the mergedData map, avoiding duplicates
                auto result = mergedData.insert(entry);
                if (!result.second) {
                    // Insertion failed, key already exists
                    logger::info("Duplicate key detected: {}", result.first->first);
                    Utilities::MsgBoxesNotifs::InGame::GeneralErr();
                }
            }
        };

        // Apply the lambda function to each Source in the vector
        for (const auto& source : sources) {
            mergeData(source);
        }

        // Return the merged map
        return mergedData;
    }

public:
    Manager(std::vector<Source>& data) : sources(data) { Init(); };

    static Manager* GetSingleton(std::vector<Source>& data) {
        static Manager singleton(data);
        return &singleton;
    }

    const char* GetType() override { return "Manager"; }

    std::vector<Source> sources;
    bool listen_menuclose = false;
    bool listen_activate = true;

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

#define ENABLE_IF_NOT_UNINSTALLED if (isUninstalled) return;

    void ActivateContainer(RE::TESObjectREFR* a_container) {
        ENABLE_IF_NOT_UNINSTALLED
        auto src = GetContainerSource(a_container->GetBaseObject()->GetFormID());
        if (!src) {
            logger::error("Could not find source for container");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return;
        }

        std::string name = a_container->GetDisplayFullName();

        // is it an already registered container?
        const auto container_refid = a_container->GetFormID();
        if (src->data.find(container_refid) != src->data.end()) {
            // Already registered
            logger::info("Already registered");
        } else {
            // Not registered
            logger::info("Not registered");
            auto ChestObjRef = FindNotMatchedChest();
            logger::info("Matched chest with refid: {} with container with refid: {}", ChestObjRef->GetFormID(),
                         container_refid);
            src->data[container_refid] = ChestObjRef->GetFormID();
        }

        current_container = a_container;

        // Round the float to 2 decimal places
        std::ostringstream stream1;
        stream1 << std::fixed << std::setprecision(2) << GetContainerChest(current_container)->GetWeightInContainer();
        std::ostringstream stream2;
        stream2 << std::fixed << std::setprecision(2) << src->capacity;

        Utilities::MsgBoxesNotifs::ShowMessageBox(name + ": " + stream1.str() + "/" + stream2.str(), buttons,
                                                  [this](unsigned int result) { this->MsgBoxCallback(result); });
    };

    void InspectItemTransfer() {
        ENABLE_IF_NOT_UNINSTALLED
        // check if container has enough capacity
        auto chest = GetContainerChest(current_container);
        auto weight_limit = GetContainerSource(current_container->GetBaseObject()->GetFormID())->capacity;
        while (chest->GetWeightInContainer() > weight_limit) {
            chest = GetContainerChest(current_container);
            auto inventory = chest->GetInventory();
            auto item = inventory.rbegin();
            auto item_obj = item->first;
            logger::info("afjksafd naaamee: {}", item_obj->GetName());
            auto item_count = item->second.first;
            auto inv_data = item->second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                logger::info("Extra list is null__");
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
            } else {
                logger::info("Removing item.5__");
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, asd->front(), player_ref);
            }
            RE::DebugNotification(std::format("{} is fully packed!", current_container->GetDisplayFullName()).c_str());
        }
    }

    void SendData() {
        ENABLE_IF_NOT_UNINSTALLED
        for (auto& src : sources) {
            for (const auto& [cont_ref, chest_ref] : src.data) {
				logger::info("Sending data: cont_form: {} cont_ref: {} chest_ref: {}", src.formid, cont_ref, chest_ref);
                SetData({src.formid, cont_ref}, chest_ref);
			}
        }
    };

    void ReceiveData() {
        ENABLE_IF_NOT_UNINSTALLED
        bool no_match = true;
        for (const auto& [cont_formref, chest_ref] : m_Data) {
            logger::info("Receiving data: cont_form: {} cont_ref: {} chest_ref: {}", cont_formref.outerKey, cont_formref.innerKey, chest_ref);
            no_match = true;
            for (auto& src : sources) {
                if (cont_formref.outerKey == src.formid) {
                    auto it = src.data.find(cont_formref.innerKey);
                    if (it != src.data.end()) {
                        logger::info("RefID {} with formid {} already exists in sources.", cont_formref.innerKey,
                                     cont_formref.outerKey);
                        Utilities::MsgBoxesNotifs::InGame::GeneralErr();
                    }
                    src.data[cont_formref.innerKey] = chest_ref;
                    logger::info("Container {} matched with chest {}.", cont_formref.innerKey, chest_ref);
                    no_match = false;
                    break;
                }
            }
            if (no_match) {
                logger::critical("FormID {} not found in sources.", cont_formref.outerKey);
                Utilities::MsgBoxesNotifs::InGame::ProblemWithContainer(std::to_string(cont_formref.outerKey));
                return InitFailed();
            }
        }
    };

#undef ENABLE_IF_NOT_UNINSTALLED

};
