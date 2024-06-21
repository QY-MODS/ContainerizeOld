#pragma once

#include "Settings.h"

using namespace Utilities::FunctionsSkyrim::Inventory;
using namespace Utilities::FunctionsSkyrim::xData;


// TODO: handled_external_conts duzgun calisiyo mu test et. save et. barrela fakeplacement yap. save et. roll back save. sonra ilerki save e geri don, bakalim fakeitemlar hala duruyor mu

class Manager : public Utilities::SaveLoadData {
    // private variables
    const std::vector<std::string> buttons = {"Open", "Take", "More...", "Close"};
    const std::vector<std::string> buttons_more = {"Rename", "Uninstall", "Back", "Close"};
    bool uiextensions_is_present = false;
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    //RE::EffectSetting* empty_mgeff = nullptr;
    
    //  maybe i dont need this by using uniqueID for new forms
    // runtime specific
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid, fake container formid}
    RE::TESObjectREFR* current_container = nullptr;
    RE::TESObjectREFR* hidden_real_ref = nullptr;

    // unowned stuff
    const RefID unownedChestOGRefID = 0x000EA29A;
    const RefID unownedChestFormID = 0x000EA299;
    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(unownedChestFormID);
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    const RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    
    std::vector<FormID> external_favs; // runtime specific, FormIDs of fake containers if faved
    std::vector<RefID> handled_external_conts; // runtime specific to prevent unnecessary checks in HandleFakePlacement
    std::map<FormID,std::string> renames;  // runtime specific, custom names for fake containers
    std::pair<FormID, RefID> real_to_sendback = {0,0};  // pff

    // private functions
    [[nodiscard]] RE::TESObjectREFR* MakeChest(const RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) {
        logger::info("Making chest");
        auto item = unownedChest->As<RE::TESBoundObject>();

        auto newPropRef = RE::TESDataHandler::GetSingleton()
                              ->CreateReferenceAtLocation(item, Pos3, {0.0f, 0.0f, 0.0f}, unownedCell, nullptr, nullptr,
                                                          nullptr, {}, true, false).get().get();
        logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                     RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
        return newPropRef;
    };

    [[nodiscard]] RE::TESObjectREFR* AddChest(const uint32_t chest_no) {
        logger::trace("Adding chest");
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

    [[nodiscard]] RE::TESObjectREFR* FindNotMatchedChest() {
        logger::trace("Finding not matched chest");
        auto& runtimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : runtimeData.references) {
            if (!ref) continue;
            if (ref->GetFormID() == unownedChestOGRefID) continue;
            if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
                size_t n_items = ref->GetInventory().size();
                bool contains_key = false;
                for (const auto& src : sources) {
                    if (src.data.count(ref->GetFormID())) {
						contains_key = true;
						break;
					}
                }
                if (!n_items && !contains_key) {
                    return ref.get();
                }
            }
        }
        return AddChest(GetNoChests());
    };

    [[nodiscard]] const uint32_t GetNoChests() {
        logger::trace("Getting number of chests");
        auto& runtimeData = unownedCell->GetRuntimeData();
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

    [[nodiscard]] const RefID GetRealContainerChest(const RefID real_refid) {
        logger::trace("GetRealContainerChest");
        if (!real_refid) {
            RaiseMngrErr("Real container refid is null");
            return 0;
        }
        for (const auto& src : sources) {
            for (const auto& [chest_refid, cont_refid] : src.data) {
				if (cont_refid == real_refid) return chest_refid;
			}
		}
        RaiseMngrErr("Real container refid not found in ChestToFakeContainer");
        return 0;
    }

    // OK. from container out in the world to linked chest
    [[nodiscard]] RE::TESObjectREFR* GetRealContainerChest(const RE::TESObjectREFR* real_container) {
        logger::trace("GetRealContainerChest");
        if (!real_container) { 
            RaiseMngrErr("Container reference is null");
            return nullptr;
        }
        RefID container_refid = real_container->GetFormID();

        auto src = GetContainerSource(real_container->GetBaseObject()->GetFormID());
        if (!src) {
            RaiseMngrErr("Could not find source for container");
            return nullptr;
        }

        if (Utilities::Functions::containsValue(src->data,container_refid)) {
            auto chest_refid = GetRealContainerChest(container_refid);
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

    [[nodiscard]] const RefID GetFakeContainerChest(const FormID fake_id) {
        logger::trace("GetFakeContainerChest");
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

    void _HandleFormDelete(const RefID chest_refid) {
        std::lock_guard<std::mutex> lock(mutex);
        auto real_formid = ChestToFakeContainer[chest_refid].outerKey;
        const auto real_item = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid);
        if (real_item) {
            const auto msg =
                std::format("Your container with name {} was deleted by the game. Will try to return your items now.",
                            real_item->GetName());
            Utilities::MsgBoxesNotifs::InGame::CustomErrMsg(msg);
        } else {
            const auto msg =
                std::format("Your container with formid {} was deleted by the game. Will try to return your items now.",
                            real_formid);
            Utilities::MsgBoxesNotifs::InGame::CustomErrMsg(msg);
        }
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        auto fake_formid = ChestToFakeContainer[chest_refid].innerKey;
        if (chest) DeRegisterChest(chest_refid);
        else Utilities::MsgBoxesNotifs::InGame::CustomErrMsg("Could not return your items.");
        RemoveItemReverse(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
    }

    // returns items in chest to player and removes entries linked to the chest from src.data and ChestToFakeContainer
    std::vector<FormID> DeRegisterChest(const RefID chest_ref) {
        logger::info("Deregistering chest");
        std::vector<FormID> removed_stuff;
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
        if (!chest) {
            RaiseMngrErr("Chest not found");
            return removed_stuff;
        }
        auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
        if (!src) {
            RaiseMngrErr("Source not found");
            return removed_stuff;
        }
        removed_stuff = RemoveAllItemsFromChest(chest, player_ref);
        // also remove the associated fake item from player or unowned chest// (<0.7.1)
        //auto fake_id = ChestToFakeContainer[chest_ref].innerKey;
        //if (fake_id) {
        //    RemoveItemReverse(player_ref, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
        //    RemoveItemReverse(unownedChestOG, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove); 
        //}
        if (!src->data.erase(chest_ref)) {
            RaiseMngrErr("Failed to remove chest refid from source");
            return removed_stuff;
        }
        if (!ChestToFakeContainer.erase(chest_ref)) {
            RaiseMngrErr("Failed to erase chest refid from ChestToFakeContainer");
            return removed_stuff;
        }
        // make sure no item is left in the chest
        if (chest->GetInventory().size()) {
            RaiseMngrErr("Chest still has items in it. Degistering failed");
            return removed_stuff;
        }   

        return removed_stuff;
    }

    // OK. from real container formid to linked source
    [[nodiscard]] Source* GetContainerSource(const FormID container_formid) {
        logger::trace("GetContainerSource");
        for (auto& src : sources) {
            if (src.formid == container_formid) {
                return &src;
            }
        }
        logger::error("Container source not found");
        return nullptr;
    };

    [[nodiscard]] RE::TESBoundObject* FakeToRealContainer(FormID fake) {
        logger::trace("FakeToRealContainer");
        for (const auto& [chest_ref, cont_forms] : ChestToFakeContainer) {
            if (cont_forms.innerKey == fake) {
                return RE::TESForm::LookupByID<RE::TESBoundObject>(cont_forms.outerKey);
            }
        }
        return nullptr;
    }

    [[nodiscard]] const int GetChestValue(RE::TESObjectREFR* a_chest) {
        logger::trace("GetChestValue");
        if (!a_chest) {
            RaiseMngrErr("Chest is null");
            return 0;
        }
		auto chest_inventory = a_chest->GetInventory();
		int total_value = 0;
		for (const auto& item : chest_inventory) {
			auto item_count = item.second.first;
			auto inv_data = item.second.second.get();
			total_value += inv_data->GetValue() * item_count;
		}
		return total_value;
	}

    template <typename T>
    void UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, const bool cloud) {
        logger::trace("UpdateFakeWV");
        
        // assumes base container is already in the chest
        if (!chest_linked || !fake_form) return RaiseMngrErr("Failed to get chest.");
        const auto fake_formid = fake_form->GetFormID();
        auto real_container = FakeToRealContainer(fake_formid);
        logger::trace("Copying from real container to fake container. Real container: {}, Fake container: {}",
                      real_container->GetFormID(), fake_formid);
        fake_form->Copy(real_container->As<T>());
        logger::trace("Copied from real container to fake container");
        logger::trace("if it was renamed, rename it back");
        if (!renames.empty() && renames.count(fake_formid)) fake_form->fullName = renames[fake_form->GetFormID()];

        if (!cloud){
            logger::trace("Setting weight for fake form");
            Utilities::FunctionsSkyrim::FormTraits<T>::SetWeight(fake_form, chest_linked->GetWeightInContainer());
        }

        auto chest_inventory = chest_linked->GetInventory();
        for (auto& [key, value] : chest_inventory) {
			logger::trace("Item: {}, Count: {}", key->GetName(), value.first);
		}

        // get the ench costoverride of fake in player inventory

        int x_0 = real_container->GetGoldValue();
        if (_other_settings[Settings::otherstuffKeys[3]]) {
            logger::trace("VALUE BEFORE {}", x_0);
            Utilities::FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
            auto temp_entry = chest_inventory.find(real_container);
            const auto extracost = EntryHasXData(temp_entry->second.second.get()) ? GetXDataCostOverride(temp_entry->second.second->extraLists->front()) : 0;
            x_0 = GetValueInContainer(chest_linked) - extracost;
            logger::trace("VALUE AFTER {}", x_0);
        }
        if (x_0 < 0) x_0 = 0;

        logger::trace("Setting weight and value for fake form");
        Utilities::FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
        logger::trace("ACTUAL VALUE {}", Utilities::FunctionsSkyrim::FormTraits<T>::GetValue(fake_form));
        
        if (!HasItem(fake_form, player_ref) || x_0 == 0) return;
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_form->GetFormID());
        const int f_0 = GetItemValue(fake_bound, player_ref);
        int f_search = f_0; 
        const int target_value = GetValueInContainer(chest_linked);
        logger::trace("Value in inventory: {}, Target value: {}", f_0, target_value);
        if (f_0 <= target_value) return;

        logger::trace("Player has the fake form, try to correct the value");
        // do binary search to find the correct value up to a tolerance
        const float tolerance = 0.01f; // 1%
        const float tolerance_val = std::max(5.0f, std::floor(tolerance * target_value) + 1);  // at least 5 
        const int max_iter = 1000;
        int curr_iter = max_iter;
        int x_search = x_0;

        logger::trace("Value in inventory: {}", f_search);
        logger::trace("x_0: {}", x_0);

        while (std::abs(f_search - target_value) > tolerance_val && curr_iter > 0) {
            if (f_search > target_value) x_search /= 2;
			else x_search += x_search/ 2;
            if (x_search < 0) return Utilities::FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
            Utilities::FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_search);
            f_search = GetItemValue(fake_bound, player_ref);
            if (f_search < x_search) return Utilities::FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
			curr_iter--;
		}

        logger::trace("iter: {}", max_iter - curr_iter);

        if (curr_iter == 0) {
            logger::warn("Max iterations reached.");
            if (std::abs(f_search - target_value) > std::abs(f_0 - target_value)){
                logger::warn("Could not find a better value for fake form");
                return Utilities::FunctionsSkyrim::FormTraits<T>::SetValue(fake_form, x_0);
            }
        }
    }

    // Updates weight and value of fake container and uses Copy and applies renaming
    void UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked, const bool cloud) {
        logger::trace("pre-UpdateFakeWV");
        if (!fake_form) return RaiseMngrErr("Fake form is null");
        std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
        if (formtype == "SCRL") UpdateFakeWV<RE::ScrollItem>(fake_form->As<RE::ScrollItem>(), chest_linked, cloud);
        else if (formtype == "ARMO") UpdateFakeWV<RE::TESObjectARMO>(fake_form->As<RE::TESObjectARMO>(), chest_linked, cloud);
        else if (formtype == "BOOK") UpdateFakeWV<RE::TESObjectBOOK>(fake_form->As<RE::TESObjectBOOK>(), chest_linked, cloud);
        else if (formtype == "INGR") UpdateFakeWV<RE::IngredientItem>(fake_form->As<RE::IngredientItem>(), chest_linked, cloud);
        else if (formtype == "MISC") UpdateFakeWV<RE::TESObjectMISC>(fake_form->As<RE::TESObjectMISC>(), chest_linked, cloud);
        else if (formtype == "WEAP") UpdateFakeWV<RE::TESObjectWEAP>(fake_form->As<RE::TESObjectWEAP>(), chest_linked, cloud);
        else if (formtype == "AMMO") UpdateFakeWV<RE::TESAmmo>(fake_form->As<RE::TESAmmo>(), chest_linked, cloud);
        else if (formtype == "SLGM") UpdateFakeWV<RE::TESSoulGem>(fake_form->As<RE::TESSoulGem>(), chest_linked, cloud);
        else if (formtype == "ALCH") UpdateFakeWV<RE::AlchemyItem>(fake_form->As<RE::AlchemyItem>(), chest_linked, cloud);
        else RaiseMngrErr(std::format("Form type not supported: {}", formtype));
        
    }

    [[nodiscard]] const bool UpdateExtrasInInventory(RE::TESObjectREFR* from_inv, const FormID from_item_formid,
                                                     RE::TESObjectREFR* to_inv, const FormID to_item_formid) {
        logger::trace("UpdateExtrasInInventory");
        auto from_item = RE::TESForm::LookupByID<RE::TESBoundObject>(from_item_formid);
        auto to_item = RE::TESForm::LookupByID<RE::TESBoundObject>(to_item_formid);
        if (!from_item || !to_item) {
            logger::error("Item bound is null");
            return false;
        }
        if (!HasItem(from_item, from_inv) || !HasItem(to_item, to_inv)) {
            logger::error("Item not found in inventory");
            return false;
        }
        auto inventory_from = from_inv->GetInventory();
        auto inventory_to = to_inv->GetInventory();
        auto entry_from = inventory_from.find(from_item);
        auto entry_to = inventory_to.find(to_item);
        RE::ExtraDataList* extralist_from = nullptr;
        RE::ExtraDataList* extralist_to = nullptr;
        RE::TESObjectREFR* ref_from = nullptr;
        RE::TESObjectREFR* ref_to = nullptr;
        bool removed_from = false;
        bool removed_to = false;
        if (entry_from->second.second && entry_from->second.second->extraLists &&
            !entry_from->second.second->extraLists->empty() && entry_from->second.second->extraLists->front()) {
            extralist_from = entry_from->second.second->extraLists->front();
        } else {
            logger::warn("No extra data list found in from item in inventory");
            return true;
            /*auto from_refhandle =
                RemoveItemReverse(from_inv, nullptr, from_item_formid, RE::ITEM_REMOVE_REASON::kDropping);
            if (!from_refhandle) {
                logger::error("Failed to remove item from inventory (from)");
                return false;
            }
            logger::trace("from_refhandle.get().get()");
            removed_from = true;
            ref_from = from_refhandle.get().get();
            extralist_from = &from_refhandle.get()->extraList;
            logger::trace("extralist_from");*/
        }
        if (!extralist_from) {
            logger::warn("Extra data list is null (from)");
            return true;
        }

        if (entry_to->second.second && entry_to->second.second->extraLists &&
            !entry_to->second.second->extraLists->empty() && entry_to->second.second->extraLists->front()) {
            extralist_to = entry_to->second.second->extraLists->front();
        } else {
            logger::warn("No extra data list found in to item in inventory");
            auto to_refhandle = RemoveItemReverse(to_inv, nullptr, to_item_formid, RE::ITEM_REMOVE_REASON::kDropping);
            if (!to_refhandle) {
                logger::error("Failed to remove item from inventory (to)");
                return false;
            }
            logger::trace("to_refhandle.get().get()");
            removed_to = true;
            ref_to = to_refhandle.get().get();
            extralist_to = &ref_to->extraList;
            logger::trace("extralist_to");
        }

        if (!extralist_to) {
            logger::error("Extra data list is null (to)");
            return false;
        }

        logger::trace("Updating extras");
        if (!UpdateExtras(extralist_from, extralist_to)) {
			logger::error("Failed to update extras");
			return false;
		}
        logger::trace("Updated extras");

        if (removed_from && !RemoveObject(ref_from, from_inv)) return false;
        if (removed_to && !RemoveObject(ref_to, to_inv)) return false;

        return true;
    }

    template <typename T>
    const FormID CreateFakeContainer(T* realcontainer, const RefID connected_chest, RE::ExtraDataList*) {
        logger::trace("CreateFakeContainer");
        T* new_form = nullptr;
        //new_form = realcontainer->CreateDuplicateForm(true, (void*)new_form)->As<T>();
        const auto real_container_formid = realcontainer->GetFormID();
        const auto real_container_editorid = Utilities::FunctionsSkyrim::GetEditorID(real_container_formid);
        if (real_container_editorid.empty()) {
			RaiseMngrErr("Failed to get editorid of real container.");
			return 0;
		}
        const auto new_form_id = DFT->FetchCreate<T>(real_container_formid, real_container_editorid, connected_chest);
        new_form = RE::TESForm::LookupByID<T>(new_form_id);

        if (!new_form) {
            RaiseMngrErr("Failed to create new form.");
            return 0;
        }
        new_form->fullName = realcontainer->GetFullName();
        logger::info("Created form with type: {}, Base ID: {:x}, Name: {}",
                     RE::FormTypeToString(new_form->GetFormType()), new_form_id, new_form->GetName());
        //unownedChestOG->AddObjectToContainer(new_form, extralist, 1, nullptr); // pre 0.7.1

        return new_form_id;
    }

    // Creates new form for fake container // pre 0.7.1: and adds it to unownedChestOG
    const FormID CreateFakeContainer(RE::TESBoundObject* container, const RefID connected_chest, RE::ExtraDataList* el) {
        logger::trace("pre-CreateFakeContainer");
        std::string formtype(RE::FormTypeToString(container->GetFormType()));
        if (formtype == "SCRL") {return CreateFakeContainer<RE::ScrollItem>(container->As<RE::ScrollItem>(), connected_chest, el);} 
        else if (formtype == "ARMO") {return CreateFakeContainer<RE::TESObjectARMO>(container->As<RE::TESObjectARMO>(), connected_chest, el);}
        else if (formtype == "BOOK") {return CreateFakeContainer<RE::TESObjectBOOK>(container->As<RE::TESObjectBOOK>(), connected_chest, el);}
        else if (formtype == "INGR") {return CreateFakeContainer<RE::IngredientItem>(container->As<RE::IngredientItem>(),connected_chest, el);}
        else if (formtype == "MISC") {return CreateFakeContainer<RE::TESObjectMISC>(container->As<RE::TESObjectMISC>(), connected_chest, el);}
        else if (formtype == "WEAP") {return CreateFakeContainer<RE::TESObjectWEAP>(container->As<RE::TESObjectWEAP>(), connected_chest, el);}
        //else if (formtype == "AMMO") {return CreateFakeContainer<RE::TESAmmo>(container->As<RE::TESAmmo>(), extralist);}
        else if (formtype == "SLGM") {return CreateFakeContainer<RE::TESSoulGem>(container->As<RE::TESSoulGem>(), connected_chest, el);} 
        else if (formtype == "ALCH") {return CreateFakeContainer<RE::AlchemyItem>(container->As<RE::AlchemyItem>(), connected_chest, el);}
        else {
            RaiseMngrErr(std::format("Form type not supported: {}", formtype));
            return 0;
        }
    }

    template <typename T>
    void Rename(const std::string new_name, T item) {
        logger::trace("Rename");
        if (!item) logger::warn("Item not found");
        else item->fullName = new_name;
    }

#define DISABLE_IF_NO_CURR_CONT if (!current_container) return RaiseMngrErr("Current container is null");

    // removes only one unit of the item
    const RE::ObjectRefHandle RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                   RE::ITEM_REMOVE_REASON reason) {

        logger::trace("RemoveItem");

        auto ref_handle = RE::ObjectRefHandle();
        
        if (!moveFrom && !moveTo) {
            RaiseMngrErr("moveFrom and moveTo are both null!");
            return ref_handle;
        }
        if (moveFrom && moveTo && moveFrom->GetFormID() == moveTo->GetFormID()) {
            logger::info("moveFrom and moveTo are the same!");
            return ref_handle;
        }
        setListenContainerChange(false);
        auto inventory = moveFrom->GetInventory();
        for (const auto& item : inventory) {
			auto item_obj = item.first;
			if (item_obj->GetFormID() == item_id) {
				auto inv_data = item.second.second.get();
				auto asd = inv_data->extraLists;
				if (!asd || asd->empty()) {
                    ref_handle =
                        moveFrom->RemoveItem(item_obj, 1, reason, nullptr, moveTo);
				} else {
                    ref_handle = moveFrom->RemoveItem(item_obj, 1, reason,
                                                      asd->front(), moveTo);
				}
                setListenContainerChange(true);
                return ref_handle;
			}
		}
        setListenContainerChange(true);
        return ref_handle;
    }

    // removes only one unit of the item
    const RE::ObjectRefHandle RemoveItemReverse(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                   RE::ITEM_REMOVE_REASON reason) {

        logger::trace("RemoveItemReverse");

        auto ref_handle = RE::ObjectRefHandle();

        if (!moveFrom && !moveTo) {
            RaiseMngrErr("moveFrom and moveTo are both null!");
            return ref_handle;
        }
        if (moveFrom && moveTo && moveFrom->GetFormID() == moveTo->GetFormID()) {
			logger::info("moveFrom and moveTo are the same!");
			return ref_handle;
		}

        logger::trace("Removing item reverse");

        setListenContainerChange(false);

        auto inventory = moveFrom->GetInventory();
        for (auto item = inventory.rbegin(); item != inventory.rend(); ++item) {
            auto item_obj = item->first;
            //if (!item_obj) RaiseMngrErr("Item object is null");
            if (item_obj && item_obj->GetFormID() == item_id) {
                auto inv_data = item->second.second.get();
                //if (!inv_data) RaiseMngrErr("Item data is null");
                auto asd = inv_data ? inv_data->extraLists : nullptr;
                if (!asd || asd->empty()) {
                    ref_handle = moveFrom->RemoveItem(item_obj, 1, reason, nullptr, moveTo);
                } else {
                    ref_handle = moveFrom->RemoveItem(item_obj, 1, reason, asd->front(), moveTo);
                }
                setListenContainerChange(true);
                return ref_handle;
            }
        }
        setListenContainerChange(true);
        return ref_handle;
    }

    // Removes the object from the world and adds it to an inventory
    [[nodiscard]] const bool RemoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container,bool owned=true) {

        logger::trace("RemoveObject");

        if (!ref) {
            logger::error("Object is null");
			return false;
        }
        if (!move2container) {
            logger::error("move2container is null");
            return false;
        }
        if (ref->IsDisabled()) logger::warn("Object is disabled");
        if (ref->IsDeleted()) logger::warn("Object is deleted");
        
        // Remove object from world
        const auto ref_bound = ref->GetBaseObject();
        if (!ref_bound) {
			logger::error("Bound object is null: {} with refid", ref->GetName(), ref->GetFormID());
			return false;
		}
        const auto ref_formid = ref_bound->GetFormID();
        
        if (owned) ref->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
        if (!PickUpItem(ref)) {
            logger::error("Failed to pick up item");
            return false;
        }
        RemoveItemReverse(player_ref, move2container, ref_formid,
                          RE::ITEM_REMOVE_REASON::kStoreInContainer);
        if (!HasItem(ref_bound, move2container)) {
            logger::error("Real container not found in move2container");
            return false;
        }
        
        return true;
    }

    std::vector<FormID> RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref = nullptr) {

        logger::trace("RemoveAllItemsFromChest");

        std::vector<FormID> removed_objects;

        if (!chest) {
            RaiseMngrErr("Chest is null");
            return removed_objects;
        }

        setListenContainerChange(false);

        auto chest_container = chest->GetContainer();
        if (!chest_container) {
            logger::error("Chest container is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return removed_objects;
        }

        if (move2ref && !move2ref->HasContainer()){
            logger::error("move2ref has no container");
			move2ref = nullptr;
        }

        auto removeReason = move2ref ? RE::ITEM_REMOVE_REASON::kStoreInContainer : RE::ITEM_REMOVE_REASON::kRemove;
        if (move2ref && move2ref->IsPlayerRef()) removeReason = RE::ITEM_REMOVE_REASON::kRemove;
        

        auto inventory = chest->GetInventory();
        for (const auto& item : inventory) {
            auto item_obj = item.first;
            if (!std::strlen(item_obj->GetName())) logger::warn("RemoveAllItemsFromChest: Item name is empty");
            auto item_count = item.second.first;
            auto inv_data = item.second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                logger::trace("Removing item: {} with count: {} and remove reason", item_obj->GetName(), item_count);
                chest->RemoveItem(item_obj, item_count, removeReason, nullptr, move2ref);
            } else {
                logger::trace("Removing item with extradata: {} with count: {}", item_obj->GetName(), item_count);
                chest->RemoveItem(item_obj, item_count, removeReason, asd->front(), move2ref);
            }
            removed_objects.push_back(item_obj->GetFormID());
        }
        
        logger::trace("Checking for fake containers in chest");
        // need to handle if a fake container was inside this chest. yani cont_ref i bu cheste bakan data varsa
        // redirectlemeliyim
        const auto chest_refid = chest->GetFormID();
        for (auto& src : sources) {
            if (!Utilities::Functions::containsValue(src.data, chest_refid)) continue;
            if (!move2ref) RaiseMngrErr("move2ref is null, but a fake container was found in the chest.");
            for (const auto& [key, value] : src.data) {
                if (value == chest_refid && key != value) {
                    logger::info(
                        "Fake container with formid {} found in chest during RemoveAllItemsFromChest. Redirecting...",
                        ChestToFakeContainer[key].innerKey);
                    if (move2ref->IsPlayerRef()) src.data[key] = key;
                    // must be sell_ref
                    else {
                        // the chest that is connected to the fake container which was inside this chest
                        HandleSell(ChestToFakeContainer[key].innerKey, move2ref->GetFormID());
                        setListenContainerChange(true);
                    }
                }
            }
        }

        setListenContainerChange(true);
        
        return removed_objects;
    };

    // Activates a container
    void Activate(RE::TESObjectREFR* a_objref) {

        logger::trace("Activate");

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

        logger::trace("ActivateChest");

        if (!chest) return RaiseMngrErr("Chest is null");
        setListenMenuClose(true);
        unownedChest->fullName = chest_name;
        logger::trace("Activating chest with name: {}", chest_name);
        logger::trace("listenclose: {}", getListenMenuClose());
        Activate(chest);
    };

    void PromptInterface() {
        DISABLE_IF_NO_CURR_CONT

        logger::trace("PromptInterface");

        // get the source corresponding to the container that we are activating
        auto src = GetContainerSource(current_container->GetBaseObject()->GetFormID());
        if (!src) return RaiseMngrErr("Could not find source for container");
        
        auto chest = GetRealContainerChest(current_container);
        auto fake_id = ChestToFakeContainer[chest->GetFormID()].innerKey;

        std::string name = renames.count(fake_id) ? renames[fake_id] : current_container->GetDisplayFullName();

        // Round the float to 2 decimal places
        std::ostringstream stream1;
        stream1 << std::fixed << std::setprecision(2) << chest->GetWeightInContainer();
        std::ostringstream stream2;
        stream2 << std::fixed << std::setprecision(2) << src->capacity;

        auto stream1_str = stream1.str();
        auto stream2_str = src->capacity > 0 ? "/" + stream2.str() : ""; 

        return Utilities::MsgBoxesNotifs::ShowMessageBox(
            name + " | W: " + stream1_str + stream2_str + " | V: " + std::to_string(GetChestValue(chest)),
            buttons,
            [this](const int result) { this->MsgBoxCallback(result); });
    }

    void SendBackReal(const FormID real_formid, RE::TESObjectREFR* chest) {
        auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
        if (!unownedChestOG) return RaiseMngrErr("MsgBoxCallback unownedChestOG is null");
        if (!HasItem(RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid), unownedChestOG))
            return RaiseMngrErr("Real container not found in unownedChestOG");
        RemoveItemReverse(unownedChestOG, chest, real_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
    }

    void MsgBoxCallback(const int result) {
        DISABLE_IF_NO_CURR_CONT
        logger::trace("Result: {}", result);

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

        // More
        if (result == 2) {
            return Utilities::MsgBoxesNotifs::ShowMessageBox("...", buttons_more,
                                                             [this](const int res) { this->MsgBoxCallbackMore(res); });
        }

        if (result == 3 || result == 1){
            const auto real_formid = current_container->GetBaseObject()->GetFormID();
            auto chest = GetRealContainerChest(current_container);
            if (!chest) return RaiseMngrErr("MsgBoxCallback Chest not found");
            SendBackReal(real_formid, chest);
            // erase real_formid from vector reals_to_sendback
            real_to_sendback = {0,0};

        }

        // Close
        if (result == 3) return;
        
        // Take
        if (result == 1) {

            setListenActivate(false);
            
            // Add fake container to player
            const auto chest_refid = GetRealContainerChest(current_container->GetFormID());
            logger::trace("Chest refid: {}", chest_refid);
            // if we already had created a fake container for this chest, then we just add it to the player's inventory
            if (!ChestToFakeContainer.count(chest_refid)) return RaiseMngrErr("Chest refid not found in ChestToFakeContainer, i.e. sth must have gone wrong during new form creation.");
            logger::trace("Fake container formid found in ChestToFakeContainer");
            
            // get the fake container from the unownedchestOG  and add it to the player's inventory
            const FormID fake_container_id = ChestToFakeContainer.at(chest_refid).innerKey;
            
            auto* fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_container_id);
            if (!fake_bound) return RaiseMngrErr("MsgBoxCallback (1): Fake bound not found");
            Utilities::FunctionsSkyrim::WorldObject::SwapObjects(current_container, fake_bound, false);

            const auto rmv_carry_boost = _other_settings[Settings::otherstuffKeys[1]];
            auto* container = current_container;
            SKSE::GetTaskInterface()->AddTask([this, chest_refid, fake_container_id, rmv_carry_boost, container]() { 
                if (!PickUpItem(container)) return RaiseMngrErr("Take:Failed to pick up container");
            
                // Update chest link (fake container is in inventory now so we replace the old refid with the chest refid -> {chestrefid:chestrefid})
                auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
                if (!src) {return RaiseMngrErr("Could not find source for container");}
                src->data[chest_refid] = chest_refid;

                logger::trace("Updating fake container V/W. Chest refid: {}, Fake container formid: {}", chest_refid, fake_container_id);
                const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
                UpdateFakeWV(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_container_id), chest, src->cloud_storage);
                logger::trace("Updated fake container V/W");

                if (rmv_carry_boost) RemoveCarryWeightBoost(fake_container_id, player_ref);
                setListenActivate(true);
                }
            );
            current_container = nullptr;

            return;
        }

        // Opening container

        // Listen for menu close
        //listen_menuclose = true;

        // Activate the unowned chest
        auto chest = GetRealContainerChest(current_container);
        if (!chest) return RaiseMngrErr("MsgBoxCallback Chest not found");
        auto fake_id = ChestToFakeContainer[chest->GetFormID()].innerKey;
        auto chest_rename = renames.count(fake_id) ? renames[fake_id].c_str() : current_container->GetName();
        ActivateChest(chest, chest_rename);
        real_to_sendback = {current_container->GetBaseObject()->GetFormID(), chest->GetFormID()};
    };

    void MsgBoxCallbackMore(const int result) {
        logger::trace("More. Result: {}", result);

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

		// Rename
        if (result == 0) {
            
            if (!uiextensions_is_present) return MsgBoxCallback(3);
            
            // Thanks and credits to Bloc: https://discord.com/channels/874895328938172446/945560222670393406/1093262407989731338
            const auto skyrimVM = RE::SkyrimVM::GetSingleton();
            auto vm = skyrimVM ? skyrimVM->impl : nullptr;
            if (vm) {
                RE::TESForm* emptyForm = NULL;
                RE::TESForm* emptyForm2 = NULL;
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                const char* menuID = "UITextEntryMenu";
                const char* container_name =
                    RE::TESForm::LookupByID<RE::TESBoundObject>(
                                      ChestToFakeContainer[GetRealContainerChest(current_container)->GetFormID()].innerKey)
                                      ->GetName();
                const char* property_name = "text";
                auto args = RE::MakeFunctionArguments(std::move(menuID), std::move(emptyForm), std::move(emptyForm2));
                auto args2 =
                    RE::MakeFunctionArguments(std::move(menuID), std::move(property_name), std::move(container_name));
                if (vm->DispatchStaticCall("UIExtensions", "SetMenuPropertyString", args2, callback)) {
                    if (vm->DispatchStaticCall("UIExtensions", "OpenMenu", args, callback)) setListenMenuClose(true);
                }
            }
            return;
		}

        // Close
        if (result == 3) return;

        // Back
        if (result == 2) return PromptInterface();

		//// Deregister
  //      if (result == 1) {
  //          if (!current_container) return RaiseMngrErr("current_container is null!");
  //          logger::trace("Deregistering real container with name {} and form id {}", current_container->GetName(), current_container->GetBaseObject()->GetFormID());
  //          auto chest = GetContainerChest(current_container);
  //          DeRegisterChest(chest->GetFormID());
  //          logger::trace("Deregistered source with name {} and form id {}", current_container->GetName(), current_container->GetBaseObject()->GetFormID());
  //          return;
		//}

        // Uninstall
        Uninstall();

    }
   

#undef DISABLE_IF_NO_CURR_CONT

    // sadece fake containerin unownedChestOG'nin icinde olmasi gereken senaryolarda kullan. (<0.7.1)
    // yani realcontainer out in the world iken.
    void HandleRegistration(RE::TESObjectREFR* a_container){
        // Create the fake container form. (<0.7.1): and put in the unownedchestOG
        // extradata gets updates when the player picks up the real container and gets the fake container from unownedchestOG (<0.7.1)

        logger::trace("HandleRegistration");

        if (!a_container) return RaiseMngrErr("Container is null");

        current_container = a_container;

        // get the source corresponding to the container that we are activating
        auto src = GetContainerSource(a_container->GetBaseObject()->GetFormID());
        if (!src) return RaiseMngrErr("Could not find source for container");

        // register the container to the source data if it is not registered
        const auto container_refid = a_container->GetFormID();
        if (!container_refid) return RaiseMngrErr("Container refid is null");
        if (!Utilities::Functions::containsValue(src->data,container_refid)) {
            // Not registered. lets find a chest to register it to
            logger::trace("Not registered");
            const auto ChestObjRef = FindNotMatchedChest();
            const auto ChestRefID = ChestObjRef->GetFormID();
            logger::info("Matched chest with refid: {} with container with refid: {}", ChestRefID, container_refid);
            if (!src->data.insert({ChestRefID, container_refid}).second) {
                return RaiseMngrErr(
                    std::format("Failed to insert chest refid {} and container refid {} into source data.", ChestRefID,
                                container_refid));
            }
            auto fake_formid = CreateFakeContainer(a_container->GetObjectReference(), ChestRefID, nullptr);
            // add to ChestToFakeContainer
            if (!ChestToFakeContainer.insert({ChestRefID, {src->formid, fake_formid}})
                     .second) {
                return RaiseMngrErr("Failed to insert chest refid and fake container refid into ChestToFakeContainer.");
            }

            // (>=0.7.1) makes a copy of the real at its current state and sends it to the linked chest
            auto temp_realref =
                Utilities::FunctionsSkyrim::WorldObject::DropObjectIntoTheWorld(a_container->GetBaseObject(), 1);
            if (!temp_realref) return RaiseMngrErr("Failed to drop real container into the world");
            if (!UpdateExtras(a_container, temp_realref)) logger::warn("Failed to update extras");
            if (!RemoveObject(temp_realref, ChestObjRef, false)) {
                return RaiseMngrErr("Failed to remove real container from unownedchestOG");
            }

            if (const auto initial_items_map = src->initial_items; !initial_items_map.empty()) {
                SKSE::GetTaskInterface()->AddTask([ChestRefID, initial_items_map] {
                    logger::trace("Adding initial items to chest");
                    const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(ChestRefID);
                    if (!chest) return;
                    for (const auto& [item, count] : initial_items_map) {
                        if (count <= 0) continue;
                        auto* bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item);
                        if (!bound) continue;
                        chest->AddObjectToContainer(bound, nullptr, count, nullptr);
                    }
                    logger::trace("Added initial items to chest");
                });
            }
        }
        // fake counterparti unownedchestOG de olmayabilir (<0.7.1)
        // cunku load gameden sonra runtimeda halletmem gerekiyo. ekle (<0.7.1)
        else {
            const auto chest_refid = GetRealContainerChest(container_refid);
            const auto real_cont_id = ChestToFakeContainer[chest_refid].outerKey;
            const auto real_cont_editorid = Utilities::FunctionsSkyrim::GetEditorID(real_cont_id);
            if (real_cont_editorid.empty()) return RaiseMngrErr("Failed to get editorid of real container.");
            // we dont care about updating other stuff at this stage since we will do it in "Take" button
            if (auto fake_cont_id = DFT->Fetch(real_cont_id, real_cont_editorid, chest_refid);!fake_cont_id) {
                logger::info("Fake container NOT found in DFT.");
                const auto real_container_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(src->formid);
                fake_cont_id = CreateFakeContainer(real_container_obj, chest_refid, nullptr);
                logger::trace("ChestToFakeContainer (chest refid: {}) before: {}", chest_refid,
                             ChestToFakeContainer[chest_refid].innerKey);
                ChestToFakeContainer[chest_refid].innerKey = fake_cont_id;
                logger::trace("ChestToFakeContainer (chest refid: {}) after: {}", chest_refid,
                             ChestToFakeContainer[chest_refid].innerKey);
            } else DFT->EditCustomID(fake_cont_id, chest_refid);
		}
    }

    [[nodiscard]] const bool IsFaved(RE::TESBoundObject* item) { return IsFavorited(item, player_ref); }

    void FaveItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
        FavoriteItem(item, inventory_owner);
    }

    void FaveItem(const FormID formid, RE::TESObjectREFR* inventory_owner) {
        FaveItem(RE::TESForm::LookupByID<RE::TESBoundObject>(formid), inventory_owner);
    }

    [[nodiscard]] const bool PickUpItem(RE::TESObjectREFR* item, const unsigned int max_try = 3) {
        logger::trace("PickUpItem");

        // std::lock_guard<std::mutex> lock(mutex);
        if (!item) {
            logger::warn("Item is null");
            return false;
        }
        auto actor = RE::PlayerCharacter::GetSingleton();
        if (!actor) {
            logger::warn("PlayerCharacter is null");
            return false;
        }

        setListenContainerChange(false);

        auto item_bound = item->GetObjectReference();
        const auto item_count = GetItemCount(item_bound, actor);
        logger::trace("Item count: {}", item_count);
        if (!item_bound) {
            logger::warn("Item bound is null");
            return false;
        }

        for (const auto& x_i : Settings::xRemove) {
            item->extraList.RemoveByType(static_cast<RE::ExtraDataType>(x_i));
        }

        item->extraList.SetOwner(RE::TESForm::LookupByID(0x07));

        unsigned int i = 0;
        while (i < max_try) {
            logger::trace("Critical: PickUpItem");
			actor->PickUpObject(item, 1, false, false);
            logger::trace("Item picked up. Checking if it is in inventory...");
            if (GetItemCount(item_bound, actor) > item_count) {
            	logger::trace("Item picked up. Took {} extra tries.", i);
                setListenContainerChange(true);
                return true;
            }
            else logger::trace("item count: {}", GetItemCount(item_bound, actor));
			i++;
		}

        setListenContainerChange(true);
        return false;
    }

    void RemoveCarryWeightBoost(const FormID item_formid,RE::TESObjectREFR* inventory_owner){

        logger::trace("RemoveCarryWeightBoost");

        auto item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid);
        if (!item_obj) return RaiseMngrErr("Item not found");
        if (!HasItem(item_obj, inventory_owner)) {
            logger::warn("Item not found in player's inventory");
            return;
        }
        auto inventory = inventory_owner->GetInventory();
        if (auto enchantment = inventory.find(item_obj)->second.second->GetEnchantment()) {
            logger::trace("Enchantment: {}", enchantment->GetName());
            // remove the enchantment from the fake container if it is carry weight boost
            for (const auto& effect : enchantment->effects) {
                logger::trace("Effect: {}", effect->baseEffect->GetName());
                logger::trace("PrimaryAV: {}", effect->baseEffect->data.primaryAV);
                logger::trace("SecondaryAV: {}", effect->baseEffect->data.secondaryAV);
                if (effect->baseEffect->data.primaryAV == RE::ActorValue::kCarryWeight) {
                    logger::trace("Removing enchantment: {}", effect->baseEffect->GetName());
                    //effect->baseEffect = empty_mgeff;
                    if (effect->effectItem.magnitude > 0) effect->effectItem.magnitude = 0;
                }
            }
        }
	}


    void InitFailed() {
        logger::critical("Failed to initialize Manager.");
        Utilities::MsgBoxesNotifs::InGame::InitErr();
        Uninstall();
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
            auto form_ = Utilities::FunctionsSkyrim::GetFormByID(src.formid,src.editorid);
            auto bound_ = src.GetBoundObject();
            if (!form_ || !bound_) {
                init_failed = true;
                logger::error("Failed to initialize Manager due to missing source: {}, {}", src.formid, src.editorid);
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

        logger::info("No of chests in cell: {}", GetNoChests());
        auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x000EA29A);
        if (!unownedChestOG || unownedChestOG->GetBaseObject()->GetFormID() != unownedChest->GetFormID() ||
            !unownedCell ||
            !unownedChest ||
            !unownedChest->As<RE::TESBoundObject>()) {
            logger::error("Failed to initialize Manager due to missing unowned chest/cell");
            init_failed = true;
        }

        if (Settings::is_pre_0_7_1) RemoveAllItemsFromChest(unownedChestOG);

        if (init_failed) return InitFailed();

        // Load also other settings...
        _other_settings = Settings::LoadOtherSettings();

        if (_other_settings[Settings::otherstuffKeys[1]]) {
            /*empty_mgeff = RE::IFormFactory::GetConcreteFormFactoryByType<RE::EffectSetting>()->Create();
            if (!empty_mgeff) {
                logger::critical("Failed to create empty mgeff.");
                init_failed = true;
            } else {
                empty_mgeff->magicItemDescription = std::string(" ");
                empty_mgeff->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
            }*/
		}

        auto data_handler = RE::TESDataHandler::GetSingleton();
        if (!data_handler) return RaiseMngrErr("Data handler is null");
        if (!data_handler->LookupModByName("UIExtensions.esp")) uiextensions_is_present = false;
        else uiextensions_is_present = true;
        

        logger::info("Manager initialized.");
    }

    // Remove all created chests while transferring the items
    void Uninstall() {
        bool uninstall_successful = true;

        logger::info("Uninstalling...");
        logger::info("No of chests in cell: {}", GetNoChests());

        // first lets get rid of the fake items from everywhere
        for (const auto& [chest_refid, real_fake_formid] : ChestToFakeContainer) {
            auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
            if (!chest) {
                uninstall_successful = false;
				logger::error("Chest not found");
				break;
            }
            auto fake_formid = real_fake_formid.innerKey;
			auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
            if (!fake_bound) {
				uninstall_successful = false;
				logger::error("Fake bound not found");
				break;
			}
            int max_try = 10;
            while (HasItemPlusCleanUp(fake_bound, chest) && max_try>0) {
				RemoveItemReverse(chest, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
                max_try--;
			}
            max_try = 10;
            while (HasItemPlusCleanUp(fake_bound, player_ref) && max_try>0) {
                RemoveItemReverse(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
                max_try--;
            }
		}

        // Delete all unowned chests and try to return all items to the player's inventory while doing that
        auto& unownedRuntimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : unownedRuntimeData.references) {
			if (!ref) continue;
            if (ref->GetFormID() == unownedChestOGRefID) continue;
            if (ref->GetBaseObject()->GetFormID() != unownedChestFormID) continue;
            if (ref->IsDisabled() && ref->IsDeleted()) continue;
            logger::info("Removing items from chest with refid {}", ref->GetFormID());
            RemoveAllItemsFromChest(ref.get(), player_ref);
			ref->Disable();
			ref->SetDelete(true);
		}

        // seems like i need to do it for the player again???????
        for (const auto& [chest_refid, real_fake_formid] : ChestToFakeContainer) {
            auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
            if (!chest) {
                uninstall_successful = false;
                logger::error("Chest not found");
                break;
            }
            auto fake_formid = real_fake_formid.innerKey;
            auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
            if (!fake_bound) {
                uninstall_successful = false;
                logger::error("Fake bound not found");
                break;
            }
            if (player_ref->GetInventory().find(fake_bound) != player_ref->GetInventory().end()) {
                RemoveItemReverse(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
            }
        }


        Reset();

        logger::info("uninstall_successful: {}", uninstall_successful);

        logger::info("No of chests in cell: {}", GetNoChests());

        if (GetNoChests() != 1) uninstall_successful = false;

        logger::info("uninstall_successful: {}", uninstall_successful);

        if (uninstall_successful) {
            // sources.clear();
            /*current_container = nullptr;
            unownedChestOG = nullptr;*/
            logger::info("Uninstall successful.");
            Utilities::MsgBoxesNotifs::InGame::UninstallSuccessful();
        } else {
            logger::critical("Uninstall failed.");
            Utilities::MsgBoxesNotifs::InGame::UninstallFailed();
        }

        setListenActivate(true); // this one stays
        setListenContainerChange(false);
        // set uninstalled flag to true
        setUninstalled(true);
    }

    void RaiseMngrErr(const std::string err_msg_ = "Error") {
        logger::error("{}", err_msg_);
        Utilities::MsgBoxesNotifs::InGame::CustomErrMsg(err_msg_);
        Utilities::MsgBoxesNotifs::InGame::GeneralErr();
        Uninstall();
    }

    // for the cases when real container is in its chest and fake container is in some other inventory (player,unownedchest,external_container)
    // DOES NOT UPDATE THE SOURCE DATA and CHESTTOFAKECONTAINER !!!
    void qTRICK__(const SourceDataKey chest_ref, const SourceDataVal cont_ref,bool fake_nonexistent = false) {
        
        logger::trace("qTrick before execute_trick");
        auto real_formid = ChestToFakeContainer[chest_ref].outerKey;
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
        auto chest_cont_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
        auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(real_formid);

        if (!chest) return RaiseMngrErr("Chest not found");
        if (!chest_cont_ref) return RaiseMngrErr("Container chest not found");
        if (!real_bound) return RaiseMngrErr("Real bound not found");
        
        // fake form nonexistent ise

        if (fake_nonexistent) {
            logger::trace("Executing trick");
            logger::trace("TRICK");

            auto old_fakeid = ChestToFakeContainer[chest_ref].innerKey;  // for external_favs
            /*auto fakeid = DFT->Fetch(real_formid, real_editorid, chest_ref);
            if (!fakeid) fakeid = CreateFakeContainer(real_bound, chest_ref, nullptr);
            else DFT->EditCustomID(fakeid, chest_ref);*/
            const auto fakeid = CreateFakeContainer(real_bound, chest_ref, nullptr);
            // load game den dolayi
            logger::trace("ChestToFakeContainer (chest refid: {}) before: {}", chest_ref,
                            ChestToFakeContainer[chest_ref].innerKey);
            ChestToFakeContainer[chest_ref].innerKey = fakeid;
            logger::trace("ChestToFakeContainer (chest refid: {}) after: {}", chest_ref,
                            ChestToFakeContainer[chest_ref].innerKey);

            // if old_fakeid is in external_favs, we need to update it with new fakeid
            auto it = std::find(external_favs.begin(), external_favs.end(), old_fakeid);
            if (it != external_favs.end()) {
                external_favs.erase(it);
                external_favs.push_back(ChestToFakeContainer[chest_ref].innerKey);
            }
            // same goes for renames
            if (renames.count(old_fakeid) && ChestToFakeContainer[chest_ref].innerKey != old_fakeid) {
                renames[ChestToFakeContainer[chest_ref].innerKey] = renames[old_fakeid];
                renames.erase(old_fakeid);
            }
        }

        logger::trace("qTrick after fake_nonexistent");
        auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // the new one (fake_nonexistent)

        logger::trace("FetchFake");

        auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
        if (!fake_bound) return RaiseMngrErr("Fake bound not found");

        if (fake_nonexistent) {
            RE::TESObjectREFR* fake_ref = Utilities::FunctionsSkyrim::WorldObject::DropObjectIntoTheWorld(fake_bound);
            if (!fake_ref) return RaiseMngrErr("Fake ref is null.");
            if (!PickUpItem(fake_ref)) return RaiseMngrErr("Failed to pick up fake container");
        }

        // Updates
        auto to_inv = fake_nonexistent ? player_ref : chest_cont_ref;
        if (!UpdateExtrasInInventory(chest, real_formid, to_inv, fake_formid)) {
            logger::error("Failed to update extras");
        }

        logger::trace("Updating FakeWV");
        const auto src = GetContainerSource(real_formid);
        if (!src) return RaiseMngrErr("Could not find source for container");
        UpdateFakeWV(fake_bound, chest, src->cloud_storage);

        // fave it if it is in external_favs
        logger::trace("Fave");
        auto it = std::find(external_favs.begin(), external_favs.end(), fake_formid);
        if (it != external_favs.end()) {
            logger::trace("Faving");
            FaveItem(fake_formid, to_inv);
        }
        // Remove carry weight boost if it has
        if (_other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_formid, to_inv);
    }

    // returns true only if the item is in the inventory with positive count. removes the item if it is in the inventory with 0 count
    [[nodiscard]] const bool HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {

        logger::trace("HasItemPlusCleanUp");

        if (HasItem(item, item_owner)) return true;
        if (HasItemEntry(item, item_owner)) {
            RemoveItemReverse(item_owner, nullptr, item->GetFormID(), RE::ITEM_REMOVE_REASON::kRemove);
            logger::trace("Item with zero count removed from player.");
        }
        return false;
    }

    void setListenActivate(const bool value) {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        listen_activate = value;
    }

    void setUninstalled(const bool value) {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        isUninstalled = value;
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

    std::unordered_map<std::string, bool> _other_settings;
    bool isUninstalled = false;

    std::mutex mutex;

    void setListenMenuClose(const bool value) {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        listen_menuclose = value;
    }

    [[nodiscard]] bool getListenMenuClose() {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        return listen_menuclose;
    }

    [[nodiscard]] bool getListenActivate() {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        return listen_activate;
    }

    [[nodiscard]] bool getListenContainerChange() {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        return listen_container_change;
    }

    void setListenContainerChange(const bool value) {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        listen_container_change = value;
    }

    [[nodiscard]] bool getUninstalled() {
        std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
        return isUninstalled;
    }
    
    [[nodiscard]] const bool IsCONT(RefID refid) {
        logger::trace("IsCONT");
        return RE::FormTypeToString(
                   RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)->GetObjectReference()->GetFormType()) == "CONT";
    }

    // Checks if realcontainer_formid is in the sources
    [[nodiscard]] const bool IsRealContainer(const FormID formid) {
        logger::trace("IsRealContainer");
        for (const auto& src : sources) {
            if (src.formid == formid) return true;
        }
        return false;
    }

    // Checks if ref has formid in the sources
    [[nodiscard]] const bool IsRealContainer(const RE::TESObjectREFR* ref) {
        logger::trace("IsRealContainer2");
        if (!ref) return false;
        if (ref->IsDisabled()) return false;
        if (ref->IsDeleted()) return false;
        const auto base = ref->GetBaseObject();
        if (!base) return false;
        return IsRealContainer(base->GetFormID());
    }

    // if the src with this formid has some data, then we say it has registry
    [[nodiscard]] const bool RealContainerHasRegistry(const FormID realcontainer_formid) {
        logger::trace("RealContainerHasRegistry");
        for (const auto& src : sources) {
            if (src.formid == realcontainer_formid) {
                if (!src.data.empty()) return true;
                break;
            }
		}
		return false;
	}

    [[nodiscard]] const bool IsARegistry(const RefID registry) {
        logger::trace("IsARegistry");
        for (const auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (cont_ref == registry) return true;
            }
		}
		return false;
	}

    // external refid is in one of the source data. unownedchests are allowed
    [[nodiscard]] const bool ExternalContainerIsRegistered(const RefID external_container_id) {
        logger::trace("ExternalContainerIsRegistered");
        if (!external_container_id) return false;
        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)->HasContainer()) {
            RaiseMngrErr("External container does not have a container.");
            return false;
        }
        return IsARegistry(external_container_id);
    }

    // external container can be found in the values of src.data
    [[nodiscard]] const bool ExternalContainerIsRegistered(const FormID fake_container_formid,
                                                           const RefID external_container_id) {
        logger::trace("ExternalContainerIsRegistered2");
        if (!external_container_id) return false;
        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)) return false;
        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)->HasContainer()) {
            RaiseMngrErr("External container does not have a container.");
            return false;
        }
        const auto real_container_formid = FakeToRealContainer(fake_container_formid)->GetFormID();
        const auto src = GetContainerSource(real_container_formid);
        if (!src) return false;
        return Utilities::Functions::containsValue(src->data,external_container_id);
    }

    [[nodiscard]] const bool IsFakeContainer(const FormID formid) {
        if (!formid) return false;
        for (const auto& [chest_ref, cont_form] : ChestToFakeContainer) {
			if (cont_form.innerKey == formid) return true;
		}
		return false;
    }

    /*bool IsFakeContainer(const RE::TESObjectREFR* ref) {
        if (!ref) return false;
        if (ref->IsDisabled()) return false;
        if (ref->IsDeleted()) return false;
        auto base = ref->GetBaseObject();
        if (!base) return false;
        return IsFakeContainer(base->GetFormID());
    }*/

    [[nodiscard]] const bool IsUnownedChest(const RefID refid) {
        logger::trace("IsUnownedChest");
        const auto* temp = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
        if (!temp) return false;
        const auto base = temp->GetBaseObject();
        return base ? base->GetFormID() == unownedChest->GetFormID() : false;
	}

    // checks if the refid is in the ChestToFakeContainer, i.e. if it is an unownedchest
    [[nodiscard]] const bool IsChest(const RefID chest_refid) { return ChestToFakeContainer.count(chest_refid) > 0; }

    const std::pair<FormID, FormID> GetRealFakePairOfChest(const RefID chest_refid) const {
        const auto& pair = ChestToFakeContainer.at(chest_refid);
        return {pair.outerKey, pair.innerKey};
    };

#define ENABLE_IF_NOT_UNINSTALLED if (isUninstalled) return;

    void UnHideReal(const FormID fakeid) { 
        ENABLE_IF_NOT_UNINSTALLED

        logger::trace("UnHideReal");

        if (!hidden_real_ref) return RaiseMngrErr("Hidden real ref is null");
        
        const auto fake_form = RE::TESForm::LookupByID(fakeid);
        if (fake_form->formFlags == 13) fake_form->formFlags = 9;

        const auto chest_refid = GetFakeContainerChest(fakeid);
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        const auto real_formid = ChestToFakeContainer[chest_refid].outerKey;
        if (hidden_real_ref->GetBaseObject()->GetFormID() != real_formid) {
            return RaiseMngrErr("Hidden real ref formid does not match the real formid");
        }
        if (!RemoveObject(hidden_real_ref, chest)) return RaiseMngrErr("Failed to UnHideReal");
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
        hidden_real_ref = nullptr;
        const auto src = GetContainerSource(real_formid);
        if (!src) return RaiseMngrErr("Could not find source for container");
        UpdateFakeWV(fake_bound, chest,src->cloud_storage);
    }

    [[nodiscard]] const bool SwapDroppedFakeContainer(RE::TESObjectREFR* ref_fake) {

        logger::trace("SwapDroppedFakeContainer");

        if (!ref_fake) {
            logger::trace("Fake container is null.");
            return false;
        }

        const auto dropped_refid = ref_fake->GetFormID();
        const auto dropped_formid = ref_fake->GetBaseObject()->GetFormID();

        const auto real_base = FakeToRealContainer(dropped_formid);
        if (!real_base) {
            logger::info("real base is null");
            return false;
        }

        /*if (auto* player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell()) {
			logger::trace("Adding persistent cell xdata to ref1");
            RE::ExtraPersistentCell* xPersistentCell = RE::BSExtraData::Create<RE::ExtraPersistentCell>();
            xPersistentCell->persistentCell = player_cell;
            logger::trace("Adding persistent cell xdata to ref2");
            ref_fake->extraList.SetExtraFlags(xPersistentCell);
            logger::trace("Added persistent cell xdata to ref.");
		}*/

        ref_fake->extraList.SetOwner(RE::TESForm::LookupByID(0x07));
        if (renames.contains(dropped_formid) && !renames[dropped_formid].empty()) {
            Utilities::FunctionsSkyrim::xData::AddTextDisplayData(&ref_fake->extraList, renames[dropped_formid]);
		}

        Utilities::FunctionsSkyrim::WorldObject::SwapObjects(ref_fake, real_base);
        // TODO: Sor bunu
        //if (ref_fake->GetAllowPromoteToPersistent()) ref_fake->inGameFormFlags.set(RE::TESObjectREFR::InGameFormFlag::kForcedPersistent);

        const auto src = GetContainerSource(real_base->GetFormID());
        if (!src) {
            RaiseMngrErr("Could not find source for container");
			return false;
		}

        const auto chest_refid = GetFakeContainerChest(dropped_formid);
        // update source data
        src->data[chest_refid] = dropped_refid;

        logger::trace("Swapped real container has refid if it didnt change in the process: {}", dropped_refid);
        return true;
    }

    void HandleContainerMenuExit() { 
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("HandleContainerMenuExit");
        if (real_to_sendback.first && real_to_sendback.second) {
            auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(real_to_sendback.second);
            if (!chest) return RaiseMngrErr("HandleContainerMenuExit: Chest is null");
            SendBackReal(real_to_sendback.first, chest);
        }
    }

    void HandleCraftingExit() { 
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("HandleCraftingExit");

        setListenContainerChange(false);

        logger::trace("Crafting menu closed");
        for (auto& src : sources) {
            for (const auto& [chest_refid, cont_refid] : src.data) {
                // we trust that the player will leave the crafting menu at some point and everything will be reverted
                if (chest_refid != cont_refid) continue;  // playerda deilse continue
                auto fake_formid = ChestToFakeContainer[chest_refid].innerKey;
                auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                if (!fake_bound) return RaiseMngrErr("Fake bound not found.");
                auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
                if (!chest) return RaiseMngrErr("Chest is null");
                if (!HasItem(fake_bound,player_ref)){
                    // it can happen when using arcane enchanter to destroy the item
                    logger::info("Player does not have fake item. Probably destroyed in arcane enchanter.");
                    RemoveItemReverse(chest, nullptr, src.formid, RE::ITEM_REMOVE_REASON::kRemove);
                    DeRegisterChest(chest_refid);
                    continue;
                }
                else if (!UpdateExtrasInInventory(player_ref, fake_formid, chest, src.formid)) {
					logger::error("Failed to update extras in player's inventory.");
					return;
				}
                
                /*auto fake_refhandle = RemoveItemReverse(player_ref, nullptr, src.formid, RE::ITEM_REMOVE_REASON::kDropping);
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;*/
                /*if (is_equipped[fake_formid]) EquipItem(fake_bound);
                if (is_faved[fake_formid]) FaveItem(fake_bound);*/
                
                //FetchFake(nullptr, fake_formid, chest_ref, real_refhandle.get().get()); // (< v0.7.1)
            }
        }

        setListenContainerChange(true);

    }

    // places fake objects in external containers after load game
    void HandleFakePlacement(RE::TESObjectREFR* external_cont) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("HandleFakePlacement");
        // if the external container is already handled (handled_external_conts) return
        const auto it = std::find(handled_external_conts.begin(), handled_external_conts.end(), external_cont->GetFormID());
        if (it != handled_external_conts.end()) return;
        
        if (!external_cont) return RaiseMngrErr("external_cont is null");
        if (IsRealContainer(external_cont)) return;
        if (!external_cont->HasContainer()) return;
        if (!ExternalContainerIsRegistered(external_cont->GetFormID())) return;
        if (IsUnownedChest(external_cont->GetFormID())) return;


        setListenContainerChange(false);

		if (!external_cont) return RaiseMngrErr("external_cont is null");
        for (auto& src : sources) {
            if (!Utilities::Functions::containsValue(src.data, external_cont->GetFormID())) continue;
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (external_cont->GetFormID() != cont_ref) continue;
                _FakePlacement(src.data[chest_ref], chest_ref, external_cont);
                // break yok cunku baska fakeler de external_cont un icinde olabilir
            }
        }
        handled_external_conts.push_back(external_cont->GetFormID());

        setListenContainerChange(true);
    }

    void HandleConsume(const FormID fake_formid) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("HandleConsume");
        // check if player has the fake item
        // sometimes player does not have the fake item but it can still be there with count = 0.
        const auto fake_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
        if (!fake_obj) return RaiseMngrErr("Fake object not found.");
        
        // the cleanup might actually not be necessary since DeRegisterChest will remove it from player
        if (HasItemPlusCleanUp(fake_obj,player_ref)) return; // false alarm
        // ok. player does not have the fake item.
        // make sure unownedOG does not have it in inventory
        // < v0.7.1
        //if (HasItemPlusCleanUp(fake_obj, unownedChestOG)) { // false alarm???
        //    logger::warn("UnownedchestOG has item.");
        //    return;
        //}

        // make also sure that the real counterpart is still in unowned
        const auto chest_refid = GetFakeContainerChest(fake_formid);
        const auto chest_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        const auto real_obj = FakeToRealContainer(fake_formid);
        if (!HasItem(real_obj, chest_ref)) return RaiseMngrErr("Real counterpart not found in unowned chest.");
        
        logger::info("Deregistering bcs Item consumed.");
        RemoveItemReverse(chest_ref, nullptr, real_obj->GetFormID(), RE::ITEM_REMOVE_REASON::kRemove);
        const auto temp_formids = DeRegisterChest(chest_refid);
        for (auto& temp_formid : temp_formids){
            if (auto temp_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(temp_formid))
                Utilities::FunctionsSkyrim::Menu::SendInventoryUpdateMessage(player_ref, temp_bound);
        }

    }

    // Register an external container (technically could be another unownedchest of another of our containers) to the source data so that chestrefid of currentcontainer -> external container
    void LinkExternalContainer(const FormID fakecontainer, const RefID externalcontainer_refid) {
        ENABLE_IF_NOT_UNINSTALLED

        setListenContainerChange(false);

        logger::trace("LinkExternalContainer");

        auto external_cont = RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer_refid);
        if (!external_cont) return RaiseMngrErr("External container not found.");

        if (!external_cont->HasContainer()) {
            return RaiseMngrErr("External container does not have a container.");
        }

        logger::trace("Linking external container.");
        const auto chest_refid = GetFakeContainerChest(fakecontainer);
        const auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
        if (!src) return RaiseMngrErr("Source not found.");
        src->data[chest_refid] = externalcontainer_refid;

        // if external container is one of ours (bcs of weight limit):
        if (IsChest(externalcontainer_refid) && src->capacity > 0) {
            logger::info("External container is one of our unowneds.");
            const auto weight_limit = src->capacity;
            if (external_cont->GetWeightInContainer() > weight_limit) {
                RemoveItemReverse(external_cont, player_ref, fakecontainer,
                           RE::ITEM_REMOVE_REASON::kStoreInContainer);
                src->data[chest_refid] = chest_refid;
            }
        }

        // add it to handled_external_conts
        //handled_external_conts.insert(externalcontainer_refid);

        // if successfully transferred to the external container, check if the fake container is faved
        if (src->data[chest_refid] != chest_refid &&
            IsFavorited(RE::TESForm::LookupByID<RE::TESBoundObject>(fakecontainer), external_cont)) {
            logger::trace("Faved item successfully transferred to external container.");
            external_favs.push_back(fakecontainer);
            
            // also update the extras in the unowned
            auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
            if (!chest) return RaiseMngrErr("LinkExternalContainer: Chest not found");
            if (!UpdateExtrasInInventory(external_cont, fakecontainer, chest,
                                         ChestToFakeContainer[chest_refid].outerKey)) {
                logger::error("Failed to update extras in linkexternal.");
            }
        }


        setListenContainerChange(true);

    }

    void UnLinkExternalContainer(const FormID fake_container_formid, const RefID externalcontainer) { 
        ENABLE_IF_NOT_UNINSTALLED

        logger::trace("UnLinkExternalContainer");

        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer)->HasContainer()) {
            return RaiseMngrErr("External container does not have a container.");
        }

        const auto real_container_formid = FakeToRealContainer(fake_container_formid)->GetFormID();
        const auto src = GetContainerSource(real_container_formid);
        if (!src) return RaiseMngrErr("Source not found.");
        const auto chest_refid = GetFakeContainerChest(fake_container_formid);

        src->data[chest_refid] = chest_refid;

        // remove it from handled_external_conts
        //handled_external_conts.erase(externalcontainer_refid);

        // remove it from external_favs
        const auto it = std::find(external_favs.begin(), external_favs.end(), fake_container_formid);
        if (it != external_favs.end()) external_favs.erase(it);

        logger::trace("Unlinked external container.");
    }

    void HandleSell(const FormID fake_container, const RefID sell_refid) {
        // assumes the sell_refid is a container
        ENABLE_IF_NOT_UNINSTALLED

        logger::trace("HandleSell");

        const auto sell_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(sell_refid);
        logger::trace("sell ref name: {}", sell_ref->GetName());
        if (!sell_ref) return RaiseMngrErr("Sell ref not found");
        // remove the fake container from vendor
        logger::trace("Removed fake container from vendor");
        // add the real container to the vendor from the unownedchest
        const auto chest_refid = GetFakeContainerChest(fake_container);
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chest) return RaiseMngrErr("Chest not found");
        RemoveItemReverse(sell_ref, nullptr, fake_container, RE::ITEM_REMOVE_REASON::kRemove);
        if (_other_settings[Settings::otherstuffKeys[3]]) RemoveAllItemsFromChest(chest, sell_ref);
        else RemoveItemReverse(chest, sell_ref, ChestToFakeContainer[chest_refid].outerKey,
                   RE::ITEM_REMOVE_REASON::kStoreInContainer);
        logger::trace("Added real container to vendor chest");
        // remove all items from the chest to the player's inventory and deregister this chest
        DeRegisterChest(chest_refid);
        logger::trace("Sell handled.");
    }


    void OnActivateContainer(RE::TESObjectREFR* a_container) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("OnActivateContainer 1 arg");
        HandleRegistration(a_container);
        
        // store it temporarily in unownedChestOG
        auto unownedChestOG = RE::TESForm::LookupByID<RE::TESObjectREFR>(unownedChestOGRefID);
        if (!unownedChestOG) return RaiseMngrErr("OnActivateContainer: unownedChestOG is null");
        auto chest = GetRealContainerChest(current_container);
        if (!chest) return RaiseMngrErr("OnActivateContainer: Chest not found");
        const auto real_formid = ChestToFakeContainer[chest->GetFormID()].outerKey;
        RemoveItemReverse(chest, unownedChestOG, real_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);

        return PromptInterface();
    };

    void ActivateContainer(const FormID fakeid, bool hide_real = false) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("ActivateContainer 2 args");
        const auto chest_refid = GetFakeContainerChest(fakeid);
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        const auto real_container_formid = FakeToRealContainer(fakeid)->GetFormID();
        const auto real_container_name = RE::TESForm::LookupByID<RE::TESBoundObject>(real_container_formid)->GetName();
        if (hide_real) {
            // if the fake container was equipped, then we need to unequip it vice versa
            logger::trace("Hiding real container {} which is in chest {}", real_container_formid, chest_refid);
            const auto fake_form = RE::TESForm::LookupByID(fakeid);
            if (fake_form->formFlags != 13) fake_form->formFlags = 13;
            const auto real_refhandle =
                RemoveItemReverse(chest, nullptr, real_container_formid, RE::ITEM_REMOVE_REASON::kDropping);
            if (!real_refhandle) return RaiseMngrErr("Real refhandle is null.");
            hidden_real_ref = real_refhandle.get().get();
		}
        
        logger::trace("Activating chest");
        const auto chest_rename = renames.count(fakeid) ? renames[fakeid].c_str() : real_container_name;
        ActivateChest(chest, chest_rename);
    };

    void RenameContainer(const std::string new_name) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("RenameContainer");
        const auto chest = GetRealContainerChest(current_container);
        if (!chest) return RaiseMngrErr("Chest not found");
        const auto fake_formid = ChestToFakeContainer[chest->GetFormID()].innerKey;
        const auto fake_form = RE::TESForm::LookupByID(fake_formid);
        if (!fake_form) return RaiseMngrErr("Fake form not found");
        const std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
        if (formtype == "SCRL") Rename(new_name, fake_form->As<RE::ScrollItem>());
        else if (formtype == "ARMO") Rename(new_name, fake_form->As<RE::TESObjectARMO>());
        else if (formtype == "BOOK") Rename(new_name, fake_form->As<RE::TESObjectBOOK>());
        else if (formtype == "INGR") Rename(new_name, fake_form->As<RE::IngredientItem>());
        else if (formtype == "MISC") Rename(new_name, fake_form->As<RE::TESObjectMISC>());
        else if (formtype == "WEAP") Rename(new_name, fake_form->As<RE::TESObjectWEAP>());
        else if (formtype == "AMMO") Rename(new_name, fake_form->As<RE::TESAmmo>());
        else if (formtype == "SLGM") Rename(new_name, fake_form->As<RE::TESSoulGem>());
        else if (formtype == "ALCH") Rename(new_name, fake_form->As<RE::AlchemyItem>());
        else logger::warn("Form type not supported: {}", formtype);

        renames[fake_formid] = new_name;
        logger::trace("Renamed fake container.");
        if (current_container){
            logger::trace("Renaming current container.");
            Utilities::FunctionsSkyrim::xData::AddTextDisplayData(&current_container->extraList, new_name);
        }

        // if reopeninitialmenu is true, then PromptInterface
        if (_other_settings[Settings::otherstuffKeys[2]]) PromptInterface();
    }

    // reverts inside the samw inventory
    void RevertEquip(const FormID fakeid) const {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("RE::TESForm::LookupByID");
        auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
        logger::trace("RE::TESForm::LookupByID___");
        auto unequip = IsEquipped(fake_bound);
        if (unequip) {
			EquipItem(fake_bound, true);
		} else {
			EquipItem(fake_bound);
		}
    }

    // reverts by sending it back to the initial inventory
    void RevertEquip(const FormID fakeid, const RefID external_container_id) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("RevertEquip");
        const auto external_container = RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id);
        if (!external_container) return RaiseMngrErr("External container not found");
        RemoveItemReverse(player_ref, external_container, fakeid, RE::ITEM_REMOVE_REASON::kRemove);
        LinkExternalContainer(fakeid, external_container_id);
    }

    // hopefully this works.
    void DropTake(const FormID realcontainer_formid, const uint32_t native_handle) { 
        // Assumes that the real container is in the player's inventory!

        logger::info("DropTaking...");

        if (!IsRealContainer(realcontainer_formid)) {
            logger::warn("Only real containers allowed");
        }
        if (!RealContainerHasRegistry(realcontainer_formid)) {
            logger::warn("Real container has no registry");
        }
        
        const auto src = GetContainerSource(realcontainer_formid);
        const auto refhandle = RemoveItemReverse(player_ref, nullptr, realcontainer_formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (refhandle.get()->GetFormID() == native_handle) {
            logger::trace("Native handle is the same as the refhandle refid");
        }
        else logger::trace("Native handle is NOT the same as the refhandle refid");
        const auto chest_refid = GetRealContainerChest(native_handle);
        auto* fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_refid].innerKey);
        Utilities::FunctionsSkyrim::WorldObject::SwapObjects(refhandle.get().get(), fake_bound,false);
        if (!PickUpItem(refhandle.get().get())) {
			RaiseMngrErr("DropTake: Failed to pick up the item.");
		}
        src->data[chest_refid] = chest_refid;
    }

    void InspectItemTransfer(const RefID chest_refid) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("InspectItemTransfer");
        // check if container has enough capacity
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        const auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
        if (!src) return RaiseMngrErr("Could not find source for container");
        if (!(src->capacity>0)) return;
        const auto weight_limit = src->capacity;

        setListenContainerChange(false);

        int max_tries = 5000;
        // TODO: Calculate the count needed to be removed
        while (chest->GetWeightInContainer() > weight_limit && max_tries>0) {
            auto inventory = chest->GetInventory();
            auto item = inventory.rbegin();
            auto item_obj = item->first;
            while (item_obj->GetWeight()<=0.001) {
				item++;
				item_obj = item->first;
			}
            auto inv_data = item->second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                chest->RemoveItem(item_obj, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, player_ref);
            } else {
                chest->RemoveItem(item_obj, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, asd->front(),
                                  player_ref);
            }
            RE::DebugNotification(
                std::format("{} is fully packed! Putting {} back.", chest->GetDisplayFullName(),
                                              item_obj->GetName())
                                      .c_str());
            max_tries--;

        }

        setListenContainerChange(true);

    }

    void Terminate() {
		ENABLE_IF_NOT_UNINSTALLED
        logger::info("Terminating manager...");
		Uninstall();
	}

    void Reset() {
        ENABLE_IF_NOT_UNINSTALLED
        logger::info("Resetting manager...");
        for (auto& src : sources) {
			src.data.clear();
		}
        ChestToFakeContainer.clear(); // we will update this in ReceiveData
        external_favs.clear(); // we will update this in ReceiveData
        renames.clear(); // we will update this in ReceiveData
        handled_external_conts.clear();
        Clear();
        //handled_external_conts.clear();
        current_container = nullptr;
        setListenMenuClose(false);
        setListenActivate(true);
        setListenContainerChange(true);
        setUninstalled(false);
        logger::info("Manager reset.");
    }

    void SendData() {
        ENABLE_IF_NOT_UNINSTALLED
        //std::lock_guard<std::mutex> lock(mutex);
        logger::info("--------Sending data---------");
        Print(); 
        Clear();
        int no_of_container = 0;
        for (auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                no_of_container++;
                bool is_equipped_x = false;
                bool is_favorited_x = false;
                if (!chest_ref) return RaiseMngrErr("Chest refid is null");
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                if (chest_ref == cont_ref) {
                    auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                    is_equipped_x = IsEquipped(fake_bound);
                    is_favorited_x = IsFaved(fake_bound);
                    auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
                    if (!chest) return RaiseMngrErr("Chest not found");
                    /*if (!UpdateExtrasInInventory(player_ref, fake_formid, chest, src.formid)) {
						logger::error("Failed to update extras in player's inventory.");
					}*/
                } 
                // check if the fake container is faved in an external container
                else {
                    auto it = std::find(external_favs.begin(), external_favs.end(), fake_formid);
                    if (it != external_favs.end()) is_favorited_x = true;
                }
                auto rename_ = renames.count(fake_formid) ? renames[fake_formid] : "";
                FormIDX fake_container_x(ChestToFakeContainer[chest_ref].innerKey, is_equipped_x, is_favorited_x, rename_);
                SetData({src.formid, chest_ref}, {fake_container_x, cont_ref});
			}
        }
        logger::info("Data sent. Number of containers: {}", no_of_container);
    };
    
    // to any possible type of container where fake might/should be stored at
    void _FakePlacement(const RefID saved_ref, const RefID chest_ref, RE::TESObjectREFR* external_cont = nullptr) {

        logger::trace("_FakePlacement");

        // bu sadece load sirasinda
        // ya playerda olcak ya da unownedlardan birinde (containerception)
        // bu ikisi disindaki seylere load_game safhasinda bisey yapamiyorum external_cont nullptr sa
        if (!external_cont && chest_ref != saved_ref && !IsChest(saved_ref)) return;

        // saved_ref should not be realcontainer out in the world!
        if (IsRealContainer(external_cont)) {
            logger::critical("saved_ref should not be realcontainer out in the world!");
            return;
        }

        // external cont mu yoksa ya playerda ya da unownedlardan birinde miyi anliyoruz
        const RefID cont_ref = chest_ref == saved_ref ? 0x14 : saved_ref; // only changing in the case of indication of player has fake container
        const auto cont_of_fakecont = external_cont ? external_cont : RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
        if (!cont_of_fakecont) return RaiseMngrErr("cont_of_fakecont not found");

        auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // dont use this again bcs it can change after qTRICK__
        auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);


        if (fake_bound && fake_bound->IsDeleted()) {
            logger::warn("Fake container with formid {} is deleted. Removing it from inventory/external_container...",
                         fake_formid);
            RemoveItemReverse(cont_of_fakecont, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
        }

        if (!fake_bound || !HasItemPlusCleanUp(fake_bound, cont_of_fakecont)) {
            qTRICK__(chest_ref, cont_ref, true);
        }
        else {
            if (!std::strlen(fake_bound->GetName())) {
                logger::warn("Fake container found in {} with empty name.", cont_of_fakecont->GetDisplayFullName());
            }
            // her ihtimale karsi datayi yeniliyorum
            //const auto real_formid = ChestToFakeContainer[chest_ref].outerKey;
            //fake_bound->Copy(RE::TESForm::LookupByID<RE::TESForm>(real_formid));
            if (fake_formid != fake_bound->GetFormID()) {
                logger::warn("Fake container formid changed from {} to {}", fake_formid, fake_bound->GetFormID());
            }
            logger::trace("Fake container found in {} with name {} and formid {}.",
                         cont_of_fakecont->GetDisplayFullName(), fake_bound->GetName(), fake_bound->GetFormID());
            qTRICK__(chest_ref, cont_ref);
        }
        
        // yani playerda deilse
        if (chest_ref != saved_ref) {
            RemoveItemReverse(player_ref, cont_of_fakecont, ChestToFakeContainer[chest_ref].innerKey,
                              RE::ITEM_REMOVE_REASON::kStoreInContainer);
        }
    }

    // places fakes according to loaded data to player or unowned chests
    void Something2(const RefID chest_ref, std::vector<RefID>& ha) {

        // ha: handled already
        if (std::find(ha.begin(), ha.end(), chest_ref) != ha.end()) return;
        logger::info("-------------------chest_ref: {} -------------------", chest_ref);
        auto connected_chests = ConnectedChests(chest_ref);
        for (const auto& connected_chest : connected_chests) {
            logger::info("Connected chest: {}", connected_chest);
            Something2(connected_chest,ha);
            //ha.push_back(connected_chest);
        }
        auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
        if (!src) return RaiseMngrErr("Could not find source for container");
        _FakePlacement(src->data[chest_ref], chest_ref);
        ha.push_back(chest_ref);
        logger::info("-------------------chest_ref: {} DONE -------------------", chest_ref);
        
    }

    void ReceiveData() {
        ENABLE_IF_NOT_UNINSTALLED
        logger::info("--------Receiving data---------");

        //std::lock_guard<std::mutex> lock(mutex);

        /*if (DFT->GetNDeleted() > 0) {
            logger::critical("ReceiveData: Deleted forms exist.");
            return RaiseMngrErr("ReceiveData: Deleted forms exist.");
        }*/

        setListenContainerChange(false);

        std::map<RefID,std::pair<bool,bool>> chest_equipped_fav;

        bool no_match;
        FormID realcontForm; RefID chestRef; FormIDX fakecontForm; RefID contRef;
        std::map<RefID, FormFormID> unmathced_chests;
        for (const auto& [realcontForm_chestRef, fakecontForm_contRef] : m_Data) {
            no_match = true;
            realcontForm = realcontForm_chestRef.outerKey;
            chestRef = realcontForm_chestRef.innerKey;
            fakecontForm = fakecontForm_contRef.outerKey;
            contRef = fakecontForm_contRef.innerKey;
            for (auto& src : sources) {
                if (realcontForm != src.formid) continue;
                if (!src.data.insert({chestRef, contRef}).second) {
                    return RaiseMngrErr(
                        std::format("RefID {} or RefID {} at formid {} already exists in sources data.", chestRef,
                                    contRef, realcontForm));
                }
                if (!ChestToFakeContainer.insert({chestRef, {realcontForm, fakecontForm.id}}).second) {
                    return RaiseMngrErr(
                        std::format("realcontForm {} with fakecontForm {} at chestref {} already exists in "
                                    "ChestToFakeContainer.",
                                    chestRef, realcontForm, fakecontForm.id));
                }
                if (!fakecontForm.name.empty()) renames[fakecontForm.id] = fakecontForm.name;
                if (chestRef == contRef) chest_equipped_fav[chestRef] = {fakecontForm.equipped, fakecontForm.favorited};
                else if (fakecontForm.favorited) external_favs.push_back(fakecontForm.id);
                no_match = false;
                break;
            }
            if (no_match) unmathced_chests[chestRef] = {realcontForm, fakecontForm.id};
        }

        // handle the unmathced chests
        // user probably changed the INI. we try to retrieve the items.
        for (const auto& [chestRef_, RealFakeForm_] : unmathced_chests) {
            logger::warn("FormID {} not found in sources.", RealFakeForm_.outerKey);
            if (_other_settings[Settings::otherstuffKeys[0]]) {
                Utilities::MsgBoxesNotifs::InGame::ProblemWithContainer(std::to_string(RealFakeForm_.outerKey));
            }
            logger::info("Deregistering chest");

            auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chestRef_);
            if (!chest) return RaiseMngrErr("Chest not found");
            RemoveAllItemsFromChest(chest, player_ref);
            // also remove the associated fake item from player or unowned chest
            auto fake_id = RealFakeForm_.innerKey;
            if (fake_id) {
                RemoveItemReverse(player_ref, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
                //RemoveItemReverse(unownedChestOG, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove); // < v0.7.1
            }
            // make sure no item is left in the chest
            if (chest->GetInventory().size()) {
                logger::critical("Chest still has items in it. Degistering failed");
                Utilities::MsgBoxesNotifs::InGame::CustomErrMsg("Items might not have been retrieved successfully.");
            }

            m_Data.erase({RealFakeForm_.outerKey, chestRef_});
        }

        Print();

        // Now i need to iterate through the chests deal with some cases
        std::vector<RefID> handled_already;
        for (const auto& [chest_ref, _] : ChestToFakeContainer) {
            if (std::find(handled_already.begin(), handled_already.end(), chest_ref) != handled_already.end()) {
				continue;
            }
            Something2(chest_ref,handled_already);
            const auto _real_fid = ChestToFakeContainer[chest_ref].outerKey;
            const auto real_editorid = Utilities::FunctionsSkyrim::GetEditorID(_real_fid);
            if (real_editorid.empty()) {
                logger::critical("Real container with formid {} has no editorid.", _real_fid);
                return RaiseMngrErr("Real container has no editorid.");
			}
            const auto _fake_fid = ChestToFakeContainer[chest_ref].innerKey;
            DFT->Reserve(_real_fid, real_editorid, _fake_fid);
		}

        // print handled_already
        logger::info("handled_already: ");
        for (const auto& ref : handled_already) {
            logger::info("{}", ref);
        }


        handled_already.clear();

        // I make the fake containers in player inventory equipped/favorited:
        logger::trace("Equipping and favoriting fake containers in player's inventory");
        auto inventory_changes = player_ref->GetInventoryChanges();
        auto entries = inventory_changes->entryList;
        for (auto it = entries->begin(); it != entries->end(); ++it){
            if (!(*it)) {
                logger::error("Entry is null. Fave-equip failed.");
				continue;
            } else if (!(*it)->object) {
				logger::error("Object is null. Fave-equip failed.");
                continue;
            }
            auto fake_formid = (*it)->object->GetFormID();
            if (IsFakeContainer(fake_formid)) {
                bool is_equipped_x = chest_equipped_fav[GetFakeContainerChest(fake_formid)].first;
                bool is_faved_x = chest_equipped_fav[GetFakeContainerChest((*it)->object->GetFormID())].second;
                if (is_equipped_x) {
                    logger::trace("Equipping fake container with formid {}", fake_formid);
                    EquipItem((*it)->object);
                    /*RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref->As<RE::Actor>(), (*it)->object, 
                        nullptr,1,(const RE::BGSEquipSlot*)nullptr,true,false,false,false);*/
				}
                if (is_faved_x) {
                    logger::trace("Favoriting fake container with formid {}", fake_formid);
                    FaveItem((*it)->object,player_ref);
                    //inventory_changes->SetFavorite((*it), (*it)->extraLists->front());
                }
			}
        }

        // need to get rid of the dynamic forms which are unused
        logger::trace("Deleting unused fake forms from bank.");
        setListenContainerChange(false);
        DFT->DeleteInactives();
        setListenContainerChange(true);
        if (DFT->GetNDeleted() > 0) {
            logger::warn("ReceiveData: Deleted forms exist. User is required to restart.");
            Utilities::MsgBoxesNotifs::InGame::CustomErrMsg(
                "It seems the configuration has changed from your previous session"
                " that requires you to restart the game."
                "DO NOT IGNORE THIS:"
                "1. Save your game."
                "2. Exit the game."
                "3. Restart the game."
                "4. Load the saved game."
                "JUST DO IT! NOW! BEFORE DOING ANYTHING ELSE!");
        }


        logger::info("--------Receiving data done---------");
        Print();
        
        current_container = nullptr;
        setListenContainerChange(true);
    };

    [[nodiscard]] const std::vector<RefID> ConnectedChests(const RefID chestRef) {
		std::vector<RefID> connected_chests;
        for (const auto& [chest_ref, cont_ref] : ChestToFakeContainer) {
            auto src = GetContainerSource(cont_ref.outerKey);
            if (!src) {
                logger::error("Source not found for formid: {}", cont_ref.outerKey);
				continue;
            }
            if (chestRef != chest_ref && src->data[chest_ref] == chestRef)
			    connected_chests.push_back(chest_ref);
		}
		return connected_chests;
	}

    void HandleFormDelete(const RefID refid) {
        ENABLE_IF_NOT_UNINSTALLED
        //std::lock_guard<std::mutex> lock(mutex);
        if (ChestToFakeContainer.contains(refid)) return _HandleFormDelete(refid);
        // the deleted reference could also be a real container out in the world.
        // in that case i need to return the items from its chest
        else {
			for (auto& src : sources) {
				for (const auto& [chest_ref, cont_ref] : src.data) {
                    if (cont_ref == refid) {
                        logger::warn("Form with refid {} is deleted. Removing it from the manager.", chest_ref);
						return _HandleFormDelete(chest_ref);
					}
				}
			}
		}
    };

    void Print() {
        
        for (const auto& src : sources) { 
            if (!src.data.empty()) {
                logger::trace("Printing............Source formid: {}", src.formid);
                Utilities::printMap(src.data); 
            }
        }
        for (const auto& [chest_ref, cont_ref] : ChestToFakeContainer) {
			logger::trace("Chest refid: {}, Real container formid: {}, Fake container formid: {}", chest_ref, cont_ref.outerKey, cont_ref.innerKey);
        }
        //Utilities::printMap(ChestToFakeContainer);
    }

    const std::vector<Source>& GetSources() const { return sources; }

#undef ENABLE_IF_NOT_UNINSTALLED

};
