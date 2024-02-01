#pragma once

#include "Settings.h"


class Manager : public Utilities::BaseFormRefIDRefID {
    // private variables
    std::vector<std::string> buttons = {"Open", "Take", "Cancel", "More..."};
    std::vector<std::string> buttons_more = {"Uninstall", "Back", "Cancel"};
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    RE::TESObjectREFR* current_container = nullptr;
    //std::map<FormID,std::vector<FormID>> newForms;
    
    //  maybe i dont need this by using uniqueID for new forms
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid, fake container formid}

    // unowned stuff
    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
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
                /*if (!no_items && !Utilities::ValueExists<SourceDataKey>(GetAllData(), ref->GetFormID())) {*/
                if (!no_items && !GetAllData().containsKey(ref->GetFormID())) {
                    return ref.get();
                }
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

    // OK. from container out in the world to linked chest
    RE::TESObjectREFR* GetContainerChest(RE::TESObjectREFR* container){
        if (!container) { 
            RaiseMngrErr("Container reference is null");
            return nullptr;
        }
        RefID container_refid = container->GetFormID();

        logger::info("Getting container source");
        auto src = GetContainerSource(container->GetBaseObject()->GetFormID());
        if (!src) {
            RaiseMngrErr("Could not find source for container");
            return nullptr;
        }

        if (src->data.containsValue(container_refid)) {
            auto chest_refid = src->data.getKey(container_refid);
            if (chest_refid == container_refid) {
                RaiseMngrErr("Chest refid is equal to container refid. This means container is in inventory, how can this be???");
                return nullptr;
            }
            if (!chest_refid) {
                RaiseMngrErr("Chest refid is null");
                return nullptr;
            };
            auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
            if (!chest) {
                RaiseMngrErr("Could not find chest");
                return nullptr;
            };
            return chest;
        } 
        else {
            RaiseMngrErr("Container refid not found in source");
            return nullptr;
        }
    }

    RefID GetContainerChest(FormID fake_id) {
        if (!fake_id) {
			RaiseMngrErr("Fake container refid is null");
			return 0;
		}
        for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
            if (cont_forms.innerKey == fake_id) return chest_ref;
        }
        RaiseMngrErr("Fake container refid not found in ChestToFakeContainer");
        return 0;
    }

    // OK. from real container formid to linked source
    Source* GetContainerSource(const FormID container_formid) {
        for (auto& src : sources) {
            if (src.formid == container_formid) {
                return &src;
            }
        }
        logger::info("Container source not found");
        return nullptr;
    };

    RE::TESBoundObject* FakeToRealContainer(FormID fake) {
        for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
            if (cont_forms.innerKey == fake) {
                return RE::TESForm::LookupByID<RE::TESBoundObject>(cont_forms.outerKey);
            }
        }
        return nullptr;
    }

    template <typename T>
    void AddFakeContainerToPlayer(T* realcontainer, RE::ExtraDataList* extralist) {
        T* new_form = nullptr;
        new_form = realcontainer->CreateDuplicateForm(true, (void*)new_form)->As<T>();
        if (!new_form) {return RaiseMngrErr("Failed to create new form.");}
        new_form->Copy(realcontainer);
        
        // update weight and value
        auto chest = GetContainerChest(current_container);
        if (!chest) {return RaiseMngrErr("Failed to get chest.");}
        auto chest_inventory = chest->GetInventory();
        auto total_value = new_form->value;
        auto total_weight = Utilities::FormTraits<T>::GetWeight(new_form);
        for (const auto& item : chest_inventory) {
			auto item_count = item.second.first;
			auto inv_data = item.second.second.get();
            total_value += inv_data->GetValue() * item_count;
			total_weight += inv_data->GetWeight() * item_count;
		}
		new_form->value = total_value;
        Utilities::FormTraits<T>::SetWeight(new_form, total_weight);
		
        // add to player with extralist
        logger::info("Adding fake container to player");
        player_ref->AddObjectToContainer(new_form, extralist, 1, nullptr);
        logger::info("Added fake container to player");

        // add to ChestToFakeContainer
        if (!ChestToFakeContainer.insert({chest->GetFormID(), {realcontainer->GetFormID(), new_form->GetFormID()}})
                 .second) {
            return RaiseMngrErr("Failed to insert chest refid and fake container refid into ChestToFakeContainer.");
        }
    }

    void AddFakeContainerToPlayer(RE::TESBoundObject* container, RE::ExtraDataList* extralist) {
        std::string formtype(RE::FormTypeToString(container->GetFormType()));
        if (formtype == "SCRL") {AddFakeContainerToPlayer<RE::ScrollItem>(container->As<RE::ScrollItem>(), extralist);}
        else if (formtype == "ARMO") {AddFakeContainerToPlayer<RE::TESObjectARMO>(container->As<RE::TESObjectARMO>(), extralist);}
        else if (formtype == "BOOK") {AddFakeContainerToPlayer<RE::TESObjectBOOK>(container->As<RE::TESObjectBOOK>(), extralist);}
        else if (formtype == "INGR") {AddFakeContainerToPlayer<RE::IngredientItem>(container->As<RE::IngredientItem>(), extralist);}
        else if (formtype == "MISC") {AddFakeContainerToPlayer<RE::TESObjectMISC>(container->As<RE::TESObjectMISC>(), extralist);}
        else if (formtype == "WEAP") {AddFakeContainerToPlayer<RE::TESObjectWEAP>(container->As<RE::TESObjectWEAP>(), extralist);}
        else if (formtype == "AMMO") {AddFakeContainerToPlayer<RE::TESAmmo>(container->As<RE::TESAmmo>(), extralist);}
        else if (formtype == "SLGM") {AddFakeContainerToPlayer<RE::TESSoulGem>(container->As<RE::TESSoulGem>(), extralist);} 
        else {RaiseMngrErr(std::format("Form type not supported: {}", formtype));}
    }

    // Unused.
    template <typename T>
    RE::TESObjectREFR* ReplaceContainerWithDuplicate(T* container_form, RE::TESObjectREFR* container_ref) {
        
        T* new_form = nullptr;
        new_form = container_form->CreateDuplicateForm(true, (void*)new_form)->As<T>();
        if (!new_form) {
            logger::error("Failed to create new form.");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return nullptr;
        }
        new_form->Copy(container_form);

        new_form->fullName = "asd";


        //newForms[container_form->GetFormID()].push_back(new_form->GetFormID());

        // get position, rotation, cell and worldspace of old form
        auto pos = container_ref->GetPosition();
        auto rot = container_ref->GetAngle();
        auto cell = container_ref->GetParentCell();
        auto worldspace = container_ref->GetWorldspace();
        
        // print worldspace formid
        auto formid = worldspace->GetFormID();
        logger::info("Worldspace formid: {}", formid);
        //look up by formid
        auto worldspace_ = RE::TESForm::LookupByID<RE::TESWorldSpace>(formid);
        if (!worldspace_) {
			logger::error("Failed to look up worldspace by formid.");
			Utilities::MsgBoxesNotifs::InGame::GeneralErr();
			return nullptr;
		}

        // create object reference for new form
        auto newform_ref = RE::TESDataHandler::GetSingleton()
            ->CreateReferenceAtLocation(new_form, pos, rot, cell, worldspace,
                                        nullptr,nullptr, {}, true, false).get().get();

        // remove old form
        container_ref->Disable();

        // print old ref formid
        logger::info("Old ref formid: {}", container_ref->GetFormID());

        // return new form ref
        return newform_ref;
    }

    void RemoveAllItemsFromChest(RE::TESObjectREFR* chest, bool transfer2player = true) {

        auto move2ref = transfer2player ? player_ref : nullptr;

        auto chest_container = chest->GetContainer();
        if (!chest_container) {
            logger::error("Chest container is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return;
        }


        auto inventory = chest->GetInventory();
        for (const auto& item : inventory) {
            auto item_obj = item.first;
            auto item_count = item.second.first;
            auto inv_data = item.second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, move2ref);
            } else {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, asd->front(), move2ref);
            }
        }
    };

    // OK.
    void Activate(RE::TESObjectREFR* a_objref) {
        if (!a_objref) {
            return RaiseMngrErr("Object reference is null");
        }
        auto a_obj = a_objref->GetBaseObject()->As<RE::TESObjectCONT>();
        if (!a_obj) {
            return RaiseMngrErr("Object is not a container");
        }
        a_obj->Activate(a_objref, player_ref, 0, a_obj, 1);
    }

    // OK.
    void ActivateChest(RE::TESObjectREFR* chest, const char* chest_name) {
        unownedChest->fullName = chest_name;
        Activate(chest);
    };

    void MsgBoxCallback(unsigned int result) {
        logger::info("Result: {}", result);
        if (!current_container) {
            return RaiseMngrErr("current_container is null!");
        }

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

        // More
        if (result == 3) {
            return Utilities::MsgBoxesNotifs::ShowMessageBox(
                "...", buttons_more, [this](unsigned int res) { this->MsgBoxCallbackMore(res); });
        }

        // Cancel
        if (result == 2) return;
        
        // Putting container to inventory
        if (result == 1) {
            
            listen_activate = false;

            // Add fake container to player
            auto curr_container_base = current_container->GetBaseObject();
            auto chest_refid = GetContainerChest(current_container)->GetFormID();
            logger::info("Chest refid: {}", chest_refid);
            // if we already had created a fake container for this chest, then we just add it to the player's inventory
            if (ChestToFakeContainer.count(chest_refid)) {
                // < this doesnt work 
    //            if (ChestToFakeContainer[chest_refid].outerKey != curr_container_base->GetFormID()) {
				//	return RaiseMngrErr("Tried to reuse new form but real form did not match saved chest.");
				//}
    //            auto new_form = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_refid].innerKey);
    //            if (!new_form) {return RaiseMngrErr("Failed to look up new form.");}
    //            // add to player with extralist
    //            logger::info("Adding fake container to player");
    //            //player_ref->AddObjectToContainer(new_form, extraL, 1, nullptr);
    //            player_ref->AddObjectToContainer(new_form, nullptr, 1, nullptr);
    //            logger::info("Added fake container to player");
                // this doesnt work >

                AddFakeContainerToPlayer(curr_container_base, nullptr);


                
            } else {
				// create new form
				//AddFakeContainerToPlayer(curr_container_base,extraL);
                AddFakeContainerToPlayer(curr_container_base, nullptr);
			}

            // Update chest link (container is in inventory now so we replace the old refid with the chest refid -> {chestrefid:chestrefid})
            logger::info("Getting container source");
            auto src = GetContainerSource(curr_container_base->GetFormID());
            if (!src) {return RaiseMngrErr("Could not find source for container");}
            if (!src->data.updateValue(chest_refid, chest_refid)) {return RaiseMngrErr("Failed to update chest link.");}

            // Remove the original container from the world
            logger::info("Removing container from world");
            //player_ref->As<RE::Actor>()->PickUpObject(current_container, 1, false, true);
            current_container->Disable();
            current_container->SetDelete(true);
            current_container = nullptr;
            //player_ref->RemoveItem(curr_container_base, 1, RE::ITEM_REMOVE_REASON::kRemove, extraL, nullptr);
            //player_ref->RemoveItem(curr_container_base, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            logger::info("Container removed from world");
            
            listen_activate = true;

            return;

            
            //current_container->SetActivationBlocked(0);
            //current_container->GetBaseObject()->Activate(current_container, player_ref, 1, nullptr, 1);
            //auto newName = static_cast<RE::ExtraTextDisplayData*>(RE::BSExtraData::Create<RE::ExtraTextDisplayData>());
            //newName->next = nullptr;
            //newName->SetName(std::to_string(asd_count).c_str());
            //asd_count++;
            //newName->ownerInstance = RE::ExtraTextDisplayData::DisplayDataType::kCustomName;
            ////newName->temperFactor = 1;
            //current_container->extraList.Add(newName);
            
            //auto new_form = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectCONT>()->Create();
            /*logger::info("New form created. Type: {}, Base ID: {:x}, Ref ID: {:x}, Name: {}",RE::FormTypeToString(new_form->GetFormType()), new_form->GetFormID(),
										 new_form->GetFormID(), new_form->GetName());*/
			//}
   //         
   //         new_form->weight = total_weight;
   //         new_form->value = total_value;
   //         
   //         player_ref
   //             ->AddObjectToContainer(new_form,nullptr,1,nullptr);

            //player_ref->As<RE::Actor>()->PickUpObject(current_container, 1, false, true);
            // Now remove the container from the player's inventory
            //RE::ExtraDataList* current_container_extraList = &current_container->extraList;
            //player_ref->RemoveItem(current_container->GetObjectReference(), 1, RE::ITEM_REMOVE_REASON::kRemove,
            //                       current_container_extraList, nullptr);
            
            /*if (current_container) logger::error("Container is not null!");
            if (current_container->GetFormID()) logger::error("Container refid is not null!!!");*/

            /*for (auto& extra : current_container->extraList) {
                logger::info("Type: {}", static_cast<std::uint8_t>(extra.GetType()));
                if (extra.GetType() == RE::ExtraDataType::kFlags) {
                	auto flags = static_cast<RE::ExtraFlags*>(&extra)->flags;
                    if (flags.any(RE::ExtraFlags::Flag::kBlockActivate)) {
                        logger::info("kBlockActivate");
                    }
                    if (flags.any(RE::ExtraFlags::Flag::kBlockPlayerActivate)) {
						logger::info("kBlockPlayerActivate");
					}
                    if (flags.any(RE::ExtraFlags::Flag::kBlockLoadEvents)) {
                    	logger::info("kBlockLoadEvents");
                    }
                    if (flags.any(RE::ExtraFlags::Flag::kBlockActivateText)) {
						logger::info("kBlockActivateText");
					}
                    if (flags.any(RE::ExtraFlags::Flag::kPlayerHasTaken)) {
                    	logger::info("kPlayerHasTaken");
                    }
                }
                if (extra.GetType() == RE::ExtraDataType::kStartingPosition) {
                    logger::info("kStartingPosition");
                }
                if (extra.GetType() == RE::ExtraDataType::kTextDisplayData) {
                    auto hmm = static_cast<RE::ExtraTextDisplayData*>(&extra);
                    logger::info("Name: {}", hmm->displayName);
                    logger::info("Name: {}", hmm->temperFactor);
				}
			}
            */
            
            // modify carry weight
            /*RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->ModActorValue(RE::ActorValue::kCarryWeight,
                                                                                    chest->GetWeightInContainer());*/
            //if (!current_container->GetFormID()) {
            //    RemoveAllItemsFromChest(chest, true);
            //    auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
            //    auto refIt = src->data.find(container_refid);
            //    if (refIt != src->data.end()) {
            //        // Remove the entry
            //        src->data.erase(refIt);
            //    } else {
            //        Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            //    }

            //} /*else current_container->SetActivationBlocked(1);*/

            
        }

        // Opening container

        // Listen for menu close
        listen_menuclose = true;

        // Activate the unowned chest
        ActivateChest(GetContainerChest(current_container), current_container->GetName());
    };

    void MsgBoxCallbackMore(unsigned int result) {
        logger::info("More");

        if (result != 0 && result != 1 && result != 2) return;

        // Cancel
        if (result == 2) return;

        // Back
        if (result == 1) {

            // get the source corresponding to the container that we are activating
            logger::info("Getting container source");
            auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
            if (!src) {
                return RaiseMngrErr("Could not find source for container");
            }

            std::string name = current_container->GetDisplayFullName();

            // Round the float to 2 decimal places
            std::ostringstream stream1;
            stream1 << std::fixed << std::setprecision(2)
                    << GetContainerChest(current_container)->GetWeightInContainer();
            std::ostringstream stream2;
            stream2 << std::fixed << std::setprecision(2) << src->capacity;

            return Utilities::MsgBoxesNotifs::ShowMessageBox(name + ": " + stream1.str() + "/" + stream2.str(), buttons,
                                                      [this](unsigned int result) { this->MsgBoxCallback(result); });

        }

        // Uninstal
        Uninstall();

    }

    void InitFailed() {
        logger::critical("Failed to initialize Manager.");
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


        // Check for invalid sources (form types etc.)
        for (auto& src : sources) {
            auto form_ = Utilities::GetFormByID(src.formid,src.editorid);
            auto bound_ = src.GetBoundObject();
            if (!form_ || !bound_) {
                init_failed = true;
                logger::error("Failed to initialize Manager due to missing source: {}",src.formid);
                break;
            }
            auto formtype_ = RE::FormTypeToString(form_->GetFormType());
            std::string formtypeString(formtype_);
            if (!Settings::AllowedFormTypes.count(formtypeString)) {
				init_failed = true;
                Utilities::MsgBoxesNotifs::InGame::FormTypeErr(form_->formID);
				logger::error("Failed to initialize Manager due to invalid source type: {}",formtype_);
				break;
			}

        }

        // Check for duplicate formids
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

        if (init_failed) InitFailed();

        logger::info("Manager initialized.");
    }

    // Remove all created chests while transferring the items and replace the created containers in the inventory with the original
    void Uninstall() {
        bool uninstall_successful = true;
        logger::info("Uninstalling...");
        logger::info("No of chests in cell: {}", GetNoChests());
        for (auto& src : sources) {
            if (!uninstall_successful) {
                break;
            }
            RE::TESObjectREFR* chest = nullptr;
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (!chest_ref) {
                    logger::info("Chest refid is null");
                    uninstall_successful = false;
                    break;
                }
                chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
                if (!chest) {
                    logger::info("Chest not found");
                    uninstall_successful = false;
                    break;
                }
                RemoveAllItemsFromChest(chest, true);
                if (chest->GetInventory().size()) {
                    uninstall_successful = false;
                    logger::info("Chest with refid {} is not empty.", chest_ref);
                    break;
                }
                logger::info("Disabling chest with refid {}", chest_ref);
                chest->Disable();
                chest->SetDelete(true);
                chest = nullptr;
                logger::info("Chest with refid {} disabled.", chest_ref);

                // if key and value are equal, then it is a fake container in the inventory. We need to replace it with
                // the original
                if (chest_ref == cont_ref) {
                    // get the refid of the fake container corresponding to the chest
                    auto fake_container = ChestToFakeContainer[chest_ref].innerKey;
                    if (!fake_container) {
                        logger::info("Fake container not found in ChestToFakeContainer");
                        uninstall_successful = false;
                        break;
                    }
                    // remove it from the player's inventory by iterating over it
                    // also add the original container to the player's inventory
                    auto player_inventory = player_ref->GetInventory();
                    for (const auto& item : player_inventory) {
                        auto item_obj = item.first;
                        auto item_formid = item_obj->GetFormID();
                        // remove the fake container from the player's inventory and add the original container
                        if (item_formid == fake_container) {
                            auto item_count = item.second.first;
                            auto inv_data = item.second.second.get();
                            auto asd = inv_data->extraLists;
                            logger::info("Removing fake container with refid {}", fake_container);
                            if (!asd || asd->empty()) {
                                // add the original container to the player's inventory
                                player_ref->AddObjectToContainer(
                                    RE::TESForm::LookupByID<RE::TESBoundObject>(src.formid), nullptr, item_count,
                                    nullptr);
                                player_ref->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                                                       nullptr);
                            } else {
                                // add the original container to the player's inventory
                                player_ref->AddObjectToContainer(
                                    RE::TESForm::LookupByID<RE::TESBoundObject>(src.formid), asd->front(), item_count,
                                    nullptr);
                                player_ref->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove,
                                                       asd->front(), nullptr);
                            }
                            logger::info("Fake container with refid {} removed.", fake_container);
                        }
                    }
                    // clear entry from ChestToFakeContainer
                    ChestToFakeContainer.erase(chest_ref);
                }

                //// Clear the entry from the source data
                //src.data.removeByKey(chest_ref);
            }
            src.data.clear();
        }

        logger::info("uninstall_successful: {}", uninstall_successful);

        logger::info("No of chests in cell: {}", GetNoChests());

        if (GetNoChests() != 1) uninstall_successful = false;

        logger::info("uninstall_successful: {}", uninstall_successful);

        if (uninstall_successful) {
            // sources.clear();
            current_container = nullptr;
            logger::info("Uninstall successful.");
            Utilities::MsgBoxesNotifs::InGame::UninstallSuccessful();
        } else {
            logger::critical("Uninstall failed.");
            Utilities::MsgBoxesNotifs::InGame::UninstallFailed();
        }

        // set uninstalled flag to true
        isUninstalled = true;
    }

    void RaiseMngrErr(const std::string err_msg_ = "Error") {
        logger::error("{}", err_msg_);
        Utilities::MsgBoxesNotifs::InGame::GeneralErr();
        Uninstall();
    }

    SourceData GetAllData() {
        // Merged map to store the result
        SourceData mergedData;
        for (const auto& src : sources) {
            if (!mergedData.insertAll(src.data)) {
            	RaiseMngrErr("Failed to insert map into mergedData.");
                return {};
            }
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

    bool isUninstalled = false;
    

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

 //   bool IsFakeContainer(FormID fake) {
	//	for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
	//		if (cont_forms.innerKey == fake) {
	//			return true;
	//		}
	//	}
	//	return false;
	//}

    bool RefIsFakeContainer(RE::TESObjectREFR* ref) {
        if (!ref) return false;
        auto base = ref->GetBaseObject();
        if (!base) return false;
        auto formid = base->GetFormID();
        for (const auto [chest_ref, cont_form] : ChestToFakeContainer) {
			if (cont_form.innerKey == formid) return true;
		}
		return false;
    }

    bool SwapDroppedFakeContainer(RE::TESObjectREFR* ref_fake) {

        // need the linked chest for updating source data
        auto chest_refid = GetContainerChest(ref_fake->GetBaseObject()->GetFormID());

        auto real_base = FakeToRealContainer(ref_fake->GetBaseObject()->GetFormID());
        if (!real_base) {
            logger::info("real base is null");
            return false;
        }
        logger::info("checkpoint 1");
        ref_fake->Disable();
        logger::info("checkpoint 2");
        RE::TESObjectREFR* real_cont = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(real_base, ref_fake->GetPosition(), ref_fake->GetAngle(),
                                            player_ref->GetParentCell(), player_ref->GetWorldspace(),nullptr, nullptr, {}, true, false).get().get();
        logger::info("checkpoint 3");
        // add extradata to the new container
        //real_cont->extraList.Add(ref_fake->extraList.GetByType(RE::ExtraDataType::kEnchantment));
        logger::info("checkpoint 4");
        ref_fake->SetDelete(true);
        ref_fake = nullptr;
        logger::info("checkpoint 5");

        // update source data
        auto src = GetContainerSource(real_base->GetFormID());
        if (!src) {
            RaiseMngrErr("Could not find source for container");
			return false;
		}
        logger::info("Updating source data with chest refid: {}", chest_refid);
        if (!src->data.updateValue(chest_refid, real_cont->GetFormID())) {
            RaiseMngrErr("Failed to update chest link.");
            return false;
        }

        ChestToFakeContainer.erase(chest_refid);

        logger::info("Swapped real container has refid: {}", real_cont->GetFormID());
        logger::info("{}", src->data.getValue(chest_refid));
        logger::info("{}", src->data.getKey(real_cont->GetFormID()));
        return true;
        
    }

    bool AllFakesInInventory() {
		auto inventory = player_ref->GetInventory();
        auto yes = true;
        for (const auto& [obj,count_entry] : inventory) {
			for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
				if (cont_forms.innerKey == obj->GetFormID()) break;
                yes = false;
			}
		}
		return yes;

	}

#define ENABLE_IF_NOT_UNINSTALLED if (isUninstalled) return;

    void ActivateContainer(RE::TESObjectREFR* a_container) {
        ENABLE_IF_NOT_UNINSTALLED

        // get the source corresponding to the container that we are activating
        logger::info("Getting container source");
        auto src = GetContainerSource(a_container->GetBaseObject()->GetFormID());
        if (!src) {
            return /*RaiseMngrErr("Could not find source for container")*/;
        }

        // register the container to the source data if it is not registered
        const auto container_refid = a_container->GetFormID();
        if (!container_refid) {
            return RaiseMngrErr("Container refid is null");
        };
        logger::info("container has refid: {}", container_refid);
        if (!src->data.containsValue(container_refid)) {
            // Not registered. lets find a chest to register it to
			logger::info("Not registered");
			const auto ChestObjRef = FindNotMatchedChest();
            const auto ChestRefID = ChestObjRef->GetFormID();
			logger::info("Matched chest with refid: {} with container with refid: {}", ChestRefID,container_refid);
            if (!src->data.insert(ChestRefID, container_refid)) {
                return RaiseMngrErr(std::format("Failed to insert chest refid {} and container refid {} into source data.", ChestRefID,container_refid));
            }
        }

        // now it is registered. Next step: Handle the activation

        current_container = a_container;
        std::string name = a_container->GetDisplayFullName();

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
        logger::info("Getting container source");
        auto weight_limit = GetContainerSource(current_container->GetBaseObject()->GetFormID())->capacity;
        while (chest->GetWeightInContainer() > weight_limit) {
            chest = GetContainerChest(current_container);
            auto inventory = chest->GetInventory();
            auto item = inventory.rbegin();
            auto item_obj = item->first;
            auto item_count = item->second.first;
            auto inv_data = item->second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
            } else {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, asd->front(), player_ref);
            }
            RE::DebugNotification(std::format("{} is fully packed!", current_container->GetDisplayFullName()).c_str());
        }
    }

    void SendData() {
        ENABLE_IF_NOT_UNINSTALLED
        for (auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (!chest_ref) {
                    return /*RaiseMngrErr("Chest refid is null")*/;
                }
                SetData({src.formid, chest_ref}, cont_ref);
			}
        }
        
    };

    // bitmedi
    void ReceiveData() {
        ENABLE_IF_NOT_UNINSTALLED
        bool no_match = true;
        FormID contForm; RefID chestRef;
        for (const auto& [contForm_chestRef, cont_ref] : m_Data) {
            no_match = true;
            contForm = contForm_chestRef.outerKey;
            chestRef = contForm_chestRef.innerKey;
            for (auto& src : sources) {
                if (contForm == src.formid) {
                    if (!src.data.insert(chestRef, cont_ref)) {
                        logger::error("RefID {} or RefID {} at formid {} already exists in sources data.", chestRef,
                                      cont_ref, contForm);
                        break;
                    }
                    // check if we need to add fake container to player
                    if (chestRef == cont_ref) {
                        auto container = RE::TESForm::LookupByID<RE::TESBoundObject>(cont_ref);
                        if (!container) {break;}
                        // add extra data list mechanism later
					    AddFakeContainerToPlayer(container,nullptr);
                    }
                    no_match = false;
                    break;
                }
            }
            if (no_match) {
                logger::critical("FormID {} not found in sources.", contForm);
                Utilities::MsgBoxesNotifs::InGame::ProblemWithContainer(std::to_string(contForm));
                return InitFailed();
            }
        }
    };

#undef ENABLE_IF_NOT_UNINSTALLED

};
