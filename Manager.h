#pragma once

#include "Settings.h"


class Manager : public Utilities::BaseFormRefIDRefID {
    // private variables
    std::vector<std::string> buttons = {"Open", "Take", "Cancel", "More..."};
    std::vector<std::string> buttons_more = {"Uninstall", "Deregister", "Back", "Cancel"};
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    RE::TESObjectREFR* current_container = nullptr;
    
    //  maybe i dont need this by using uniqueID for new forms
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid, fake container formid}

    // unowned stuff
    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    RE::TESObjectREFR* unownedChestOG = nullptr;

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

    void UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to) {
        // Enchantment
        auto extraList = copy_from->extraList;
        /*auto extraList_len = Utilities::GetListLength<RE::BSExtraData>(extraList);
        logger::info("ExtraList length: {}", extraList_len);
        if (!extraList_len) return;*/
        // for (const auto& extra : extraList) {
        //	if (!*extra) continue;
        //          if (extra.GetType() == RE::ExtraDataType::kEnchantment) {
        //		auto enchantment = static_cast<RE::ExtraEnchantment*>(*extra);
        //		copy_to->extraList.Add(RE::ExtraEnchantment::Create(enchantment->enchantment,enchantment->charge));
        //	}
        //}
        // for (auto& extra : extraList) {
        //    logger::info("Type: {}", static_cast<std::uint8_t>(extra.GetType()));
        //    if (extra.GetType() == RE::ExtraDataType::kFlags) {
        //        auto flags = static_cast<RE::ExtraFlags*>(&extra)->flags;
        //        if (flags.any(RE::ExtraFlags::Flag::kBlockActivate)) {
        //            logger::info("kBlockActivate");
        //        }
        //        if (flags.any(RE::ExtraFlags::Flag::kBlockPlayerActivate)) {
        //            logger::info("kBlockPlayerActivate");
        //        }
        //        if (flags.any(RE::ExtraFlags::Flag::kBlockLoadEvents)) {
        //            logger::info("kBlockLoadEvents");
        //        }
        //        if (flags.any(RE::ExtraFlags::Flag::kBlockActivateText)) {
        //            logger::info("kBlockActivateText");
        //        }
        //        if (flags.any(RE::ExtraFlags::Flag::kPlayerHasTaken)) {
        //            logger::info("kPlayerHasTaken");
        //        }
        //    }
        //    if (extra.GetType() == RE::ExtraDataType::kStartingPosition) {
        //        logger::info("kStartingPosition");
        //    }
        //    if (extra.GetType() == RE::ExtraDataType::kTextDisplayData) {
        //        auto hmm = static_cast<RE::ExtraTextDisplayData*>(&extra);
        //        logger::info("Name: {}", hmm->displayName);
        //        logger::info("Name: {}", hmm->temperFactor);
        //    }
        //}
    }

    unsigned int GetChestValue(RE::TESObjectREFR* a_chest) {
		auto chest_inventory = a_chest->GetInventory();
		unsigned int total_value = 0;
		for (const auto& item : chest_inventory) {
			auto item_count = item.second.first;
			auto inv_data = item.second.second.get();
			total_value += inv_data->GetValue() * item_count;
		}
		return total_value;
	}

    template <typename T>
    void UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked) {
        // assumes base container is already in the chest
        if (!chest_linked) return RaiseMngrErr("Failed to get chest.");
        fake_form->value = GetChestValue(chest_linked);
        Utilities::FormTraits<T>::SetWeight(fake_form, chest_linked->GetWeightInContainer());
    }

    void UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked) {
        std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
        if (formtype == "SCRL") {
            UpdateFakeWV<RE::ScrollItem>(fake_form->As<RE::ScrollItem>(), chest_linked);
        } else if (formtype == "ARMO") {
            UpdateFakeWV<RE::TESObjectARMO>(fake_form->As<RE::TESObjectARMO>(), chest_linked);
        } else if (formtype == "BOOK") {
            UpdateFakeWV<RE::TESObjectBOOK>(fake_form->As<RE::TESObjectBOOK>(), chest_linked);
        } else if (formtype == "INGR") {
            UpdateFakeWV<RE::IngredientItem>(fake_form->As<RE::IngredientItem>(), chest_linked);
        } else if (formtype == "MISC") {
            UpdateFakeWV<RE::TESObjectMISC>(fake_form->As<RE::TESObjectMISC>(), chest_linked);
        } else if (formtype == "WEAP") {
            UpdateFakeWV<RE::TESObjectWEAP>(fake_form->As<RE::TESObjectWEAP>(), chest_linked);
        } else if (formtype == "AMMO") {
            UpdateFakeWV<RE::TESAmmo>(fake_form->As<RE::TESAmmo>(), chest_linked);
        } else if (formtype == "SLGM") {
            UpdateFakeWV<RE::TESSoulGem>(fake_form->As<RE::TESSoulGem>(), chest_linked);
        } else {
            RaiseMngrErr(std::format("Form type not supported: {}", formtype));
        }
    }

    // uses current_container
    template <typename T>
    void AddFakeContainerToPlayer(T* realcontainer, RE::ExtraDataList* extralist) {
        T* new_form = nullptr;
        new_form = realcontainer->CreateDuplicateForm(true, (void*)new_form)->As<T>();
        if (!new_form) {return RaiseMngrErr("Failed to create new form.");}
        new_form->Copy(realcontainer);
        new_form->fullName = realcontainer->GetFullName();
        logger::info("Created form with type: {}, Base ID: {:x}, Ref ID: {:x}, Name: {}",RE::FormTypeToString(new_form->GetFormType()), new_form->GetFormID(),
										 new_form->GetFormID(), new_form->GetName());
        
        
        // add to player with extralist
        logger::info("Adding fake container to player");
        player_ref->AddObjectToContainer(new_form, extralist, 1, nullptr);
        logger::info("Added fake container to player");

        auto chest = GetContainerChest(current_container);

        // add to ChestToFakeContainer
        if (!ChestToFakeContainer.insert({chest->GetFormID(), {realcontainer->GetFormID(), new_form->GetFormID()}}).second) {
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

    RE::ObjectRefHandle RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                   RE::ITEM_REMOVE_REASON reason) { 
        listen_container_change = false;
        RE::ObjectRefHandle ref_handle;
        auto inventory = moveFrom->GetInventory();
        for (const auto& item : inventory) {
			auto item_obj = item.first;
			if (item_obj->GetFormID() == item_id) {
				auto item_count = item.second.first;
				auto inv_data = item.second.second.get();
				auto asd = inv_data->extraLists;
				if (!asd || asd->empty()) {
                    ref_handle =
                        moveFrom->RemoveItem(item_obj, item_count, reason, nullptr, moveTo);
				} else {
                    logger::info("Removing item with extra data");
                    ref_handle = moveFrom->RemoveItem(item_obj, item_count, reason,
                                                      asd->front(), moveTo);
				}
				listen_container_change = true;
				return ref_handle;
			}
		}
        listen_container_change = true;
        return ref_handle;
    }

    void RemoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container) {
        // Remove object from world
        if (!ref || ref->IsDisabled() || ref->IsDeleted()) return RaiseMngrErr("Object is null or disabled or deleted");
        
        ref->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
        logger::info("owner: {}", ref->GetOwner()->GetName());
        logger::info("owner: {}", ref->GetOwner()->GetFormID());

        logger::info("Picking up");
        player_ref->As<RE::Actor>()->PickUpObject(ref, 1, false, true);
        logger::info("Removing");
        player_ref->RemoveItem(ref->GetBaseObject(), 1, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr,
                                move2container);
        logger::info("Removed");
        // get index of the item in the move2container_container inventory
        //auto move2container_container = move2container->GetContainer();
        //if (!move2container_container) return RaiseMngrErr("move2container_container is null");
        //auto index = move2container_container->GetContainerObjectIndex(ref->GetBaseObject(), 1);
        //// Check if the optional has a value
        //if (index.has_value()) {
        //    // Access the value if it exists
        //    logger::info("Index: {}", index.value());
        //} else {
        //    std::cout << "Optional is empty." << std::endl;
        //}
        //logger::info("item count in move2container_container: {}", move2container_container->numContainerObjects);
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

    // uses current_container
    void PromptInterface() {
		if (!current_container) return RaiseMngrErr("current_container is null!");
        // get the source corresponding to the container that we are activating
        logger::info("Getting container source");
        auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
        if (!src) return RaiseMngrErr("Could not find source for container");
        
        std::string name = current_container->GetDisplayFullName();

        // Round the float to 2 decimal places
        std::ostringstream stream1;
        stream1 << std::fixed << std::setprecision(2) << GetContainerChest(current_container)->GetWeightInContainer();
        std::ostringstream stream2;
        stream2 << std::fixed << std::setprecision(2) << src->capacity;

        return Utilities::MsgBoxesNotifs::ShowMessageBox(
            name + " | W: " + stream1.str() + "/" + stream2.str() + " | V: " + std::to_string(GetChestValue(GetContainerChest(current_container))), buttons,
            [this](unsigned int result) { this->MsgBoxCallback(result); });
    }

    // uses current_container
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
            auto chest = GetContainerChest(current_container);
            auto chest_refid = chest->GetFormID();
            FormID fake_container_id;
            logger::info("Chest refid: {}", chest_refid);

            // if we already had created a fake container for this chest, then we just add it to the player's inventory
            if (ChestToFakeContainer.count(chest_refid)) {
                logger::info("Fake container found in ChestToFakeContainer");
                // it must be in the unownedchestOG
                if (!unownedChestOG) return RaiseMngrErr("unownedChestOG is null");
                // get the fake container from the unownedchestOG  and add it to the player's inventory
                fake_container_id = ChestToFakeContainer[chest_refid].innerKey;
                RemoveItem(unownedChestOG, player_ref, fake_container_id,RE::ITEM_REMOVE_REASON::kRemove);
            } else {
				// create new form
				//AddFakeContainerToPlayer(curr_container_base,extraL);
                logger::info("Fake container NOT found in ChestToFakeContainer");
                AddFakeContainerToPlayer(curr_container_base, nullptr);
			}

            // Update chest link (container is in inventory now so we replace the old refid with the chest refid -> {chestrefid:chestrefid})
            logger::info("Getting container source");
            auto src = GetContainerSource(curr_container_base->GetFormID());
            if (!src) {return RaiseMngrErr("Could not find source for container");}
            if (!src->data.updateValue(chest_refid, chest_refid)) {return RaiseMngrErr("Failed to update chest link.");}

            // Send the original container from the world to the linked chest
            logger::info("Removing container from world");
            RemoveObject(current_container, chest);
            logger::info("Container removed from world");

            // update weight and value
            fake_container_id = ChestToFakeContainer[chest_refid].innerKey;
            auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_container_id);
            UpdateFakeWV(fake_cont, chest);
            
            current_container = nullptr;
            listen_activate = true;

            return;
        }

        // Opening container

        // Listen for menu close
        listen_menuclose = true;

        // Activate the unowned chest
        ActivateChest(GetContainerChest(current_container), current_container->GetName());
    };

    void MsgBoxCallbackMore(unsigned int result) {
        logger::info("More");

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

        // Cancel
        if (result == 3) return;

        // Back
        if (result == 2) return PromptInterface();

		// Deregister
        if (result == 1) {
            if (!current_container) return RaiseMngrErr("current_container is null!");
            int index = 0;
            FormID fake_formid;
            for (const auto& src : sources)  {
                if (src.formid == current_container->GetBaseObject()->GetFormID()) {
                    for (const auto& [chest_ref, cont_ref] : src.data) {
                        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
                        if (!chest) return RaiseMngrErr("Chest not found");
                        RemoveAllItemsFromChest(chest, true);
                        // I set chest == cont_ref when there is no ref to a real container (this happens after Take) <=> the fake container is in the player's inventory
                        fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                        if (chest_ref == cont_ref) RemoveItem(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
                        ChestToFakeContainer.erase(chest_ref);
					}
                    break;
                }
                ++index;
			}
            current_container->SetActivationBlocked(false);
            current_container = nullptr;
            sources.erase(sources.begin() + index);
            logger::info("Deregistered source with name {} and form id {}", current_container->GetName(), current_container->GetBaseObject()->GetFormID());
            return;
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


        // make chest in the cell
        //MakeChest(unownedChestPos);


        logger::info("No of chests in cell: {}", GetNoChests());


        // Check if unowned chest is in the cell and get its ref
        auto runtimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : runtimeData.references) {
			if (!ref) continue;
			if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
				unownedChestOG = ref.get();
				break;
			}
		}

        if (!unownedChestOG || !unownedCell || !unownedChest || !unownedChest->As<RE::TESBoundObject>()) {
            logger::error("Failed to initialize Manager due to missing unowned chest/cell");
            init_failed = true;
        }

        if (init_failed) InitFailed();

        logger::info("Manager initialized.");
    }

    // Remove all created chests while transferring the items
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

                // if key and value are equal, then it is a fake container in the player inventory
                if (chest_ref == cont_ref) ChestToFakeContainer.erase(chest_ref);
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
            unownedChestOG = nullptr;
            logger::info("Uninstall successful.");
            Utilities::MsgBoxesNotifs::InGame::UninstallSuccessful();
        } else {
            logger::critical("Uninstall failed.");
            Utilities::MsgBoxesNotifs::InGame::UninstallFailed();
        }


        //listen_activate = false; not this one
        listen_menuclose = false;
        listen_container_change = false;

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
    bool listen_container_change = true;

    bool isUninstalled = false;
    

    bool RefIsContainer(RE::TESObjectREFR* ref) {
        if (!ref) return false;
        if (ref->IsDisabled()) return false;
        if (ref->IsDeleted()) return false;
        auto base = ref->GetBaseObject();
        if (!base) return false;
        auto formid = base->GetFormID();
        for (const auto& src : sources) {
            if (src.formid == formid) return true;
        }
        return false;
    }

    bool RefIsFakeContainer(RE::TESObjectREFR* ref) {
        if (!ref) return false;
        if (ref->IsDisabled()) return false;
        if (ref->IsDeleted()) return false;
        auto base = ref->GetBaseObject();
        if (!base) return false;
        auto formid = base->GetFormID();
        for (const auto [chest_ref, cont_form] : ChestToFakeContainer) {
			if (cont_form.innerKey == formid) return true;
		}
		return false;
    }

    bool SwapDroppedFakeContainer(RE::TESObjectREFRPtr ref_fake) {

        // need the linked chest for updating source data
        auto chest_refid = GetContainerChest(ref_fake->GetBaseObject()->GetFormID());
        auto chestObjRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);

        auto real_base = FakeToRealContainer(ref_fake->GetBaseObject()->GetFormID());
        if (!real_base) {
            logger::info("real base is null");
            return false;
        }
        
        logger::info("checkpoint 2");
        RE::ObjectRefHandle real_cont_handle = RemoveItem(chestObjRef, nullptr, real_base->GetFormID(),
                                                RE::ITEM_REMOVE_REASON::kDropping);
        auto real_cont = real_cont_handle.get();
        real_cont->SetParentCell(ref_fake->GetParentCell());
        real_cont->SetPosition(ref_fake->GetPosition());
        
        logger::info("checkpoint 1");
        RemoveObject(ref_fake.get(),unownedChestOG);

        logger::info("checkpoint 3");
        real_cont->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
        logger::info("owner: {}", real_cont->GetOwner()->GetName());
        logger::info("owner: {}", real_cont->GetOwner()->GetFormID());

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

        logger::info("Swapped real container has refid: {}", real_cont->GetFormID());
        logger::info("{}", src->data.getValue(chest_refid));
        logger::info("{}", src->data.getKey(real_cont->GetFormID()));
        return true;
    }

    bool AllFakesInInventory() {
        // check if all fake containers are in the player's inventory
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
        return PromptInterface();

    };

    // uses current_container
    void InspectItemTransfer() {
        ENABLE_IF_NOT_UNINSTALLED
        // check if container has enough capacity
        auto chest = GetContainerChest(current_container);
        logger::info("Getting container source");
        auto weight_limit = GetContainerSource(current_container->GetBaseObject()->GetFormID())->capacity;
        while (chest->GetWeightInContainer() > weight_limit) {
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
            RE::DebugNotification(std::format("{} is fully packed! Removed {}.",
                                              current_container->GetDisplayFullName(), item_obj->GetName())
                                      .c_str());

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
