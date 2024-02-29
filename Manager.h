#pragma once

#include "Settings.h"


// TODO: handled_external_conts duzgun calisiyo mu test et. save et. barrela fakeplacement yap. save et. roll back save. sonra ilerki save e geri don, bakalim fakeitemlar hala duruyor mu

class Manager : public Utilities::BaseFormRefIDFormRefIDX {
    // private variables
    const std::vector<std::string> buttons = {"Open", "Take", "Cancel", "More..."};
    const std::vector<std::string> buttons_more = {"Uninstall", "Back", "Cancel"};
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
    
    //  maybe i dont need this by using uniqueID for new forms
    // runtime specific
    std::map<RefID,FormFormID> ChestToFakeContainer; // chest refid -> {real container formid, fake container formid}
    RE::TESObjectREFR* current_container = nullptr;
    RE::TESObjectREFR* hidden_real_ref = nullptr;

    // unowned stuff
    RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
    RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
    //RE::TESObjectCELL* unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000FE47B);  // cwquartermastercontainers
    //RE::TESObjectCONT* unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000A0DB5); // playerhousechestnew
    const RE::NiPoint3 unownedChestPos = {1986.f, 1780.f, 6784.f};
    RE::TESObjectREFR* unownedChestOG = nullptr;
    
    //std::unordered_set<RefID> handled_external_conts; // runtime specific
    std::map<FormID,bool> is_equipped; // runtime specific and used by handlecrafting
    std::map<FormID, bool> is_faved;  // runtime specific and used by handlecrafting
    std::vector<FormID> external_favs; // runtime specific, FormIDs of fake containers if faved


    // private functions
    RE::TESObjectREFR* MakeChest(const RE::NiPoint3 Pos3 = {0.0f, 0.0f, 0.0f}) {
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
        auto& runtimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : runtimeData.references) {
            if (!ref) continue;
            if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
                size_t n_items = ref->GetInventory().size();
                bool contains_key = false;
                for (const auto& src : sources) {
                    if (src.data.count(ref->GetFormID())) {
						contains_key = true;
						break;
					}
                }
                /*if (!no_items && !Utilities::ValueExists<SourceDataKey>(GetAllData(), ref->GetFormID())) {*/
                /*if (!no_items && !GetAllData().containsKey(ref->GetFormID())) {*/
                if (!n_items && !contains_key) {
                    return ref.get();
                }
            }
        }
        return AddChest(GetNoChests());
    };

    uint32_t GetNoChests() {
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

    RefID GetRealContainerChest(const RefID real_refid) {
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
    RE::TESObjectREFR* GetContainerChest(const RE::TESObjectREFR* real_container) {
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

        if (Utilities::containsValue(src->data,container_refid)) {
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

    RefID GetFakeContainerChest(const FormID fake_id) {
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

    // returns items in chest to player and removes entries linked to the chest from src.data and ChestToFakeContainer
    void DeRegisterChest(const RefID chest_ref) {
        logger::info("Deregistering chest");
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
        if (!chest) return RaiseMngrErr("Chest not found");
        auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
        if (!src) return RaiseMngrErr("Source not found");
        RemoveAllItemsFromChest(chest, true);
        // also remove the associated fake item from player or unowned chest
        auto fake_id = ChestToFakeContainer[chest_ref].innerKey;
        if (fake_id) {
            RemoveItemReverse(player_ref, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
            RemoveItemReverse(unownedChestOG, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
        }
        if (!src->data.erase(chest_ref)) return RaiseMngrErr("Failed to remove chest refid from source");
        if (!ChestToFakeContainer.erase(chest_ref)) return RaiseMngrErr("Failed to erase chest refid from ChestToFakeContainer");
        // make sure no item is left in the chest
        if (chest->GetInventory().size()) RaiseMngrErr("Chest still has items in it. Degistering failed");
    }

    // OK. from real container formid to linked source
    Source* GetContainerSource(const FormID container_formid) {
        for (auto& src : sources) {
            if (src.formid == container_formid) {
                return &src;
            }
        }
        logger::error("Container source not found");
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

    void UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to) {
        if (!copy_from || !copy_to) return RaiseMngrErr("copy_from or copy_to is null");
        // Enchantment
        if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
            logger::info("Enchantment found");
            auto enchantment =
                static_cast<RE::ExtraEnchantment*>(copy_from->GetByType(RE::ExtraDataType::kEnchantment));
            if (enchantment) {
                RE::ExtraEnchantment* enchantment_fake = RE::BSExtraData::Create<RE::ExtraEnchantment>();
                // log the associated actor value
                logger::info("Associated actor value: {}", enchantment->enchantment->GetAssociatedSkill());
                enchantment_fake->enchantment = enchantment->enchantment;
                enchantment_fake->charge = enchantment->charge;
                enchantment_fake->removeOnUnequip = enchantment->removeOnUnequip;
                copy_to->Add(enchantment_fake);
            } else
                RaiseMngrErr("Failed to get enchantment from copy_from");
        }
        // Health
        if (copy_from->HasType(RE::ExtraDataType::kHealth)) {
            logger::info("Health found");
            auto health = static_cast<RE::ExtraHealth*>(copy_from->GetByType(RE::ExtraDataType::kHealth));
            if (health) {
                RE::ExtraHealth* health_fake = RE::BSExtraData::Create<RE::ExtraHealth>();
                health_fake->health = health->health;
                copy_to->Add(health_fake);
            } else
                RaiseMngrErr("Failed to get health from copy_from");
        }
        // Rank
        if (copy_from->HasType(RE::ExtraDataType::kRank)) {
            logger::info("Rank found");
            auto rank = static_cast<RE::ExtraRank*>(copy_from->GetByType(RE::ExtraDataType::kRank));
            if (rank) {
                RE::ExtraRank* rank_fake = RE::BSExtraData::Create<RE::ExtraRank>();
                rank_fake->rank = rank->rank;
                copy_to->Add(rank_fake);
            } else
                RaiseMngrErr("Failed to get rank from copy_from");
        }
        // TimeLeft
        if (copy_from->HasType(RE::ExtraDataType::kTimeLeft)) {
            logger::info("TimeLeft found");
            auto timeleft = static_cast<RE::ExtraTimeLeft*>(copy_from->GetByType(RE::ExtraDataType::kTimeLeft));
            if (timeleft) {
                RE::ExtraTimeLeft* timeleft_fake = RE::BSExtraData::Create<RE::ExtraTimeLeft>();
                timeleft_fake->time = timeleft->time;
                copy_to->Add(timeleft_fake);
            } else
                RaiseMngrErr("Failed to get timeleft from copy_from");
        }
        // Charge
        if (copy_from->HasType(RE::ExtraDataType::kCharge)) {
            logger::info("Charge found");
            auto charge = static_cast<RE::ExtraCharge*>(copy_from->GetByType(RE::ExtraDataType::kCharge));
            if (charge) {
                RE::ExtraCharge* charge_fake = RE::BSExtraData::Create<RE::ExtraCharge>();
                charge_fake->charge = charge->charge;
                copy_to->Add(charge_fake);
            } else
                RaiseMngrErr("Failed to get charge from copy_from");
        }
        // Scale
        if (copy_from->HasType(RE::ExtraDataType::kScale)) {
            logger::info("Scale found");
            auto scale = static_cast<RE::ExtraScale*>(copy_from->GetByType(RE::ExtraDataType::kScale));
            if (scale) {
                RE::ExtraScale* scale_fake = RE::BSExtraData::Create<RE::ExtraScale>();
                scale_fake->scale = scale->scale;
                copy_to->Add(scale_fake);
            } else
                RaiseMngrErr("Failed to get scale from copy_from");
        }
        // UniqueID
        if (copy_from->HasType(RE::ExtraDataType::kUniqueID)) {
            logger::info("UniqueID found");
            auto uniqueid = static_cast<RE::ExtraUniqueID*>(copy_from->GetByType(RE::ExtraDataType::kUniqueID));
            if (uniqueid) {
                RE::ExtraUniqueID* uniqueid_fake = RE::BSExtraData::Create<RE::ExtraUniqueID>();
                uniqueid_fake->baseID = uniqueid->baseID;
                uniqueid_fake->uniqueID = uniqueid->uniqueID;
                copy_to->Add(uniqueid_fake);
            } else
                RaiseMngrErr("Failed to get uniqueid from copy_from");
        }
        // Poison
        if (copy_from->HasType(RE::ExtraDataType::kPoison)) {
            logger::info("Poison found");
            auto poison = static_cast<RE::ExtraPoison*>(copy_from->GetByType(RE::ExtraDataType::kPoison));
            if (poison) {
                RE::ExtraPoison* poison_fake = RE::BSExtraData::Create<RE::ExtraPoison>();
                poison_fake->poison = poison->poison;
                poison_fake->count = poison->count;
                copy_to->Add(poison_fake);
            } else
                RaiseMngrErr("Failed to get poison from copy_from");
        }
        // ObjectHealth
        if (copy_from->HasType(RE::ExtraDataType::kObjectHealth)) {
            logger::info("ObjectHealth found");
            auto objhealth =
                static_cast<RE::ExtraObjectHealth*>(copy_from->GetByType(RE::ExtraDataType::kObjectHealth));
            if (objhealth) {
                RE::ExtraObjectHealth* objhealth_fake = RE::BSExtraData::Create<RE::ExtraObjectHealth>();
                objhealth_fake->health = objhealth->health;
                copy_to->Add(objhealth_fake);
            } else
                RaiseMngrErr("Failed to get objecthealth from copy_from");
        }
        // Light
        if (copy_from->HasType(RE::ExtraDataType::kLight)) {
            logger::info("Light found");
            auto light = static_cast<RE::ExtraLight*>(copy_from->GetByType(RE::ExtraDataType::kLight));
            if (light) {
                RE::ExtraLight* light_fake = RE::BSExtraData::Create<RE::ExtraLight>();
                light_fake->lightData = light->lightData;
                copy_to->Add(light_fake);
            } else
                RaiseMngrErr("Failed to get light from copy_from");
        }
        // Radius
        if (copy_from->HasType(RE::ExtraDataType::kRadius)) {
            logger::info("Radius found");
            auto radius = static_cast<RE::ExtraRadius*>(copy_from->GetByType(RE::ExtraDataType::kRadius));
            if (radius) {
                RE::ExtraRadius* radius_fake = RE::BSExtraData::Create<RE::ExtraRadius>();
                radius_fake->radius = radius->radius;
                copy_to->Add(radius_fake);
            } else
                RaiseMngrErr("Failed to get radius from copy_from");
        }
        // Sound (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kSound)) {
            logger::info("Sound found");
            auto sound = static_cast<RE::ExtraSound*>(copy_from->GetByType(RE::ExtraDataType::kSound));
            if (sound) {
                RE::ExtraSound* sound_fake = RE::BSExtraData::Create<RE::ExtraSound>();
                sound_fake->handle = sound->handle;
                copy_to->Add(sound_fake);
            } else
                RaiseMngrErr("Failed to get radius from copy_from");
        }*/
        // LinkedRef (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kLinkedRef)) {
            logger::info("LinkedRef found");
            auto linkedref =
                static_cast<RE::ExtraLinkedRef*>(copy_from->GetByType(RE::ExtraDataType::kLinkedRef));
            if (linkedref) {
                RE::ExtraLinkedRef* linkedref_fake = RE::BSExtraData::Create<RE::ExtraLinkedRef>();
                linkedref_fake->linkedRefs = linkedref->linkedRefs;
                copy_to->Add(linkedref_fake);
            } else
                RaiseMngrErr("Failed to get linkedref from copy_from");
        }*/
        // Horse
        if (copy_from->HasType(RE::ExtraDataType::kHorse)) {
            logger::info("Horse found");
            auto horse = static_cast<RE::ExtraHorse*>(copy_from->GetByType(RE::ExtraDataType::kHorse));
            if (horse) {
                RE::ExtraHorse* horse_fake = RE::BSExtraData::Create<RE::ExtraHorse>();
                horse_fake->horseRef = horse->horseRef;
                copy_to->Add(horse_fake);
            } else
                RaiseMngrErr("Failed to get horse from copy_from");
        }
        // Hotkey
        if (copy_from->HasType(RE::ExtraDataType::kHotkey)) {
            logger::info("Hotkey found");
            auto hotkey = static_cast<RE::ExtraHotkey*>(copy_from->GetByType(RE::ExtraDataType::kHotkey));
            if (hotkey) {
                RE::ExtraHotkey* hotkey_fake = RE::BSExtraData::Create<RE::ExtraHotkey>();
                hotkey_fake->hotkey = hotkey->hotkey;
                copy_to->Add(hotkey_fake);
            } else
                RaiseMngrErr("Failed to get hotkey from copy_from");
        }
        // Weapon Attack Sound (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kWeaponAttackSound)) {
            logger::info("WeaponAttackSound found");
            auto weaponattacksound = static_cast<RE::ExtraWeaponAttackSound*>(
                copy_from->GetByType(RE::ExtraDataType::kWeaponAttackSound));
            if (weaponattacksound) {
                RE::ExtraWeaponAttackSound* weaponattacksound_fake =
                    RE::BSExtraData::Create<RE::ExtraWeaponAttackSound>();
                weaponattacksound_fake->handle = weaponattacksound->handle;
                copy_to->Add(weaponattacksound_fake);
            } else
                RaiseMngrErr("Failed to get weaponattacksound from copy_from");
        }*/
        // Activate Ref (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kActivateRef)) {
            logger::info("ActivateRef found");
            auto activateref =
                static_cast<RE::ExtraActivateRef*>(copy_from->GetByType(RE::ExtraDataType::kActivateRef));
            if (activateref) {
                RE::ExtraActivateRef* activateref_fake = RE::BSExtraData::Create<RE::ExtraActivateRef>();
                activateref_fake->parents = activateref->parents;
                activateref_fake->activateFlags = activateref->activateFlags;
            } else
                RaiseMngrErr("Failed to get activateref from copy_from");
        }*/
        // TextDisplayData
        if (copy_from->HasType(RE::ExtraDataType::kTextDisplayData)) {
            logger::info("TextDisplayData found");
            auto textdisplaydata =
                static_cast<RE::ExtraTextDisplayData*>(copy_from->GetByType(RE::ExtraDataType::kTextDisplayData));
            if (textdisplaydata) {
                RE::ExtraTextDisplayData* textdisplaydata_fake = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
                textdisplaydata_fake->displayName = textdisplaydata->displayName;
                textdisplaydata_fake->displayNameText = textdisplaydata->displayNameText;
                textdisplaydata_fake->ownerQuest = textdisplaydata->ownerQuest;
                textdisplaydata_fake->ownerInstance = textdisplaydata->ownerInstance;
                textdisplaydata_fake->temperFactor = textdisplaydata->temperFactor;
                textdisplaydata_fake->customNameLength = textdisplaydata->customNameLength;
                copy_to->Add(textdisplaydata_fake);
            } else
                RaiseMngrErr("Failed to get textdisplaydata from copy_from");
        }
        // Soul
        if (copy_from->HasType(RE::ExtraDataType::kSoul)) {
            logger::info("Soul found");
            auto soul = static_cast<RE::ExtraSoul*>(copy_from->GetByType(RE::ExtraDataType::kSoul));
            if (soul) {
                RE::ExtraSoul* soul_fake = RE::BSExtraData::Create<RE::ExtraSoul>();
                soul_fake->soul = soul->soul;
                copy_to->Add(soul_fake);
            } else
                RaiseMngrErr("Failed to get soul from copy_from");
        }
        // Flags (OK)
        if (copy_from->HasType(RE::ExtraDataType::kFlags)) {
            logger::info("Flags found");
            auto flags = static_cast<RE::ExtraFlags*>(copy_from->GetByType(RE::ExtraDataType::kFlags));
            if (flags) {
                SKSE::stl::enumeration<RE::ExtraFlags::Flag, std::uint32_t> flags_fake;
                if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivate))
                    flags_fake.set(RE::ExtraFlags::Flag::kBlockActivate);
                if (flags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate))
                    flags_fake.set(RE::ExtraFlags::Flag::kBlockPlayerActivate);
                if (flags->flags.all(RE::ExtraFlags::Flag::kBlockLoadEvents))
                    flags_fake.set(RE::ExtraFlags::Flag::kBlockLoadEvents);
                if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivateText))
                    flags_fake.set(RE::ExtraFlags::Flag::kBlockActivateText);
                if (flags->flags.all(RE::ExtraFlags::Flag::kPlayerHasTaken))
                    flags_fake.set(RE::ExtraFlags::Flag::kPlayerHasTaken);
                // RE::ExtraFlags* flags_fake = RE::BSExtraData::Create<RE::ExtraFlags>();
                // flags_fake->flags = flags->flags;
                // copy_to->Add(flags_fake);
            } else
                RaiseMngrErr("Failed to get flags from copy_from");
        }
        // Lock (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kLock)) {
            logger::info("Lock found");
            auto lock = static_cast<RE::ExtraLock*>(copy_from->GetByType(RE::ExtraDataType::kLock));
            if (lock) {
                RE::ExtraLock* lock_fake = RE::BSExtraData::Create<RE::ExtraLock>();
                lock_fake->lock = lock->lock;
                copy_to->Add(lock_fake);
            } else
                RaiseMngrErr("Failed to get lock from copy_from");
        }*/
        // Teleport (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kTeleport)) {
            logger::info("Teleport found");
            auto teleport =
                static_cast<RE::ExtraTeleport*>(copy_from->GetByType(RE::ExtraDataType::kTeleport));
            if (teleport) {
                RE::ExtraTeleport* teleport_fake = RE::BSExtraData::Create<RE::ExtraTeleport>();
                teleport_fake->teleportData = teleport->teleportData;
                copy_to->Add(teleport_fake);
            } else
                RaiseMngrErr("Failed to get teleport from copy_from");
        }*/
        // LockList (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kLockList)) {
            logger::info("LockList found");
            auto locklist =
                static_cast<RE::ExtraLockList*>(copy_from->GetByType(RE::ExtraDataType::kLockList));
            if (locklist) {
                RE::ExtraLockList* locklist_fake = RE::BSExtraData::Create<RE::ExtraLockList>();
                locklist_fake->list = locklist->list;
                copy_to->Add(locklist_fake);
            } else
                RaiseMngrErr("Failed to get locklist from copy_from");
        }*/
        // OutfitItem (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kOutfitItem)) {
            logger::info("OutfitItem found");
            auto outfititem =
                static_cast<RE::ExtraOutfitItem*>(copy_from->GetByType(RE::ExtraDataType::kOutfitItem));
            if (outfititem) {
                RE::ExtraOutfitItem* outfititem_fake = RE::BSExtraData::Create<RE::ExtraOutfitItem>();
                outfititem_fake->id = outfititem->id;
                copy_to->Add(outfititem_fake);
            } else
                RaiseMngrErr("Failed to get outfititem from copy_from");
        }*/
        // CannotWear (Disabled)
        /*if (copy_from->HasType(RE::ExtraDataType::kCannotWear)) {
            logger::info("CannotWear found");
            auto cannotwear =
                static_cast<RE::ExtraCannotWear*>(copy_from->GetByType(RE::ExtraDataType::kCannotWear));
            if (cannotwear) {
                RE::ExtraCannotWear* cannotwear_fake = RE::BSExtraData::Create<RE::ExtraCannotWear>();
                copy_to->Add(cannotwear_fake);
            } else
                RaiseMngrErr("Failed to get cannotwear from copy_from");
        }*/
        // Ownership (OK)
        if (copy_from->HasType(RE::ExtraDataType::kOwnership)) {
            logger::info("Ownership found");
            auto ownership = static_cast<RE::ExtraOwnership*>(copy_from->GetByType(RE::ExtraDataType::kOwnership));
            if (ownership) {
                logger::info("length of fake extradatalist: {}", copy_to->GetCount());
                RE::ExtraOwnership* ownership_fake = RE::BSExtraData::Create<RE::ExtraOwnership>();
                ownership_fake->owner = ownership->owner;
                copy_to->Add(ownership_fake);
                logger::info("length of fake extradatalist: {}", copy_to->GetCount());
            } else
                RaiseMngrErr("Failed to get ownership from copy_from");
        }
    }

    void UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to) {
        if (!copy_from || !copy_to) return RaiseMngrErr("copy_from or copy_to is null");
        auto* copy_from_extralist = &copy_from->extraList;
        auto* copy_to_extralist = &copy_to->extraList;
        UpdateExtras(copy_from_extralist, copy_to_extralist);
    }

    /*void UpdateExtras(RE::InventoryEntryData* copy_from, RE::InventoryEntryData* copy_to) {
        auto extralists_from = copy_from->extraLists;
        if (extralists_from->empty()) return;
        auto extralists_to = copy_to->extraLists;
        if (extralists_to->empty()) {
            auto empty_extralist = new RE::ExtraDataList();
            copy_to->AddExtraList(empty_extralist);
        }
    }*/

    int GetChestValue(RE::TESObjectREFR* a_chest) {
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
    void UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked) {
        
        // assumes base container is already in the chest
        if (!chest_linked || !fake_form) return RaiseMngrErr("Failed to get chest.");
        auto real_container = FakeToRealContainer(fake_form->GetFormID());
        logger::info("Copying from real container to fake container. Real container: {}, Fake container: {}",
					 real_container->GetFormID(), fake_form->GetFormID());
        fake_form->Copy(real_container->As<T>());
        
        logger::info("Setting weight and value for fake form");
        auto realcontainer_val = real_container->GetGoldValue();
        Utilities::FormTraits<T>::SetValue(fake_form, realcontainer_val);
        Utilities::FormTraits<T>::SetWeight(fake_form, chest_linked->GetWeightInContainer());
    }

    void UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked) {

        std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
        if (formtype == "SCRL") UpdateFakeWV<RE::ScrollItem>(fake_form->As<RE::ScrollItem>(), chest_linked);
        else if (formtype == "ARMO") UpdateFakeWV<RE::TESObjectARMO>(fake_form->As<RE::TESObjectARMO>(), chest_linked);
        else if (formtype == "BOOK") UpdateFakeWV<RE::TESObjectBOOK>(fake_form->As<RE::TESObjectBOOK>(), chest_linked);
        else if (formtype == "INGR") UpdateFakeWV<RE::IngredientItem>(fake_form->As<RE::IngredientItem>(), chest_linked);
        else if (formtype == "MISC") UpdateFakeWV<RE::TESObjectMISC>(fake_form->As<RE::TESObjectMISC>(), chest_linked);
        else if (formtype == "WEAP") UpdateFakeWV<RE::TESObjectWEAP>(fake_form->As<RE::TESObjectWEAP>(), chest_linked);
        else if (formtype == "AMMO") UpdateFakeWV<RE::TESAmmo>(fake_form->As<RE::TESAmmo>(), chest_linked);
        else if (formtype == "SLGM") UpdateFakeWV<RE::TESSoulGem>(fake_form->As<RE::TESSoulGem>(), chest_linked);
        else if (formtype == "ALCH") UpdateFakeWV<RE::AlchemyItem>(fake_form->As<RE::AlchemyItem>(), chest_linked);
        else RaiseMngrErr(std::format("Form type not supported: {}", formtype));
        
    }

    template <typename T>
    FormID CreateFakeContainer(T* realcontainer, RE::ExtraDataList* extralist) {
        T* new_form = nullptr;
        new_form = realcontainer->CreateDuplicateForm(true, (void*)new_form)->As<T>();

        if (!new_form) {
            RaiseMngrErr("Failed to create new form.");
            return 0;
        }
        new_form->Copy(realcontainer);

        new_form->fullName = realcontainer->GetFullName();
        logger::info("Created form with type: {}, Base ID: {:x}, Ref ID: {:x}, Name: {}",
                     RE::FormTypeToString(new_form->GetFormType()), new_form->GetFormID(), new_form->GetFormID(),
                     new_form->GetName());
        unownedChestOG->AddObjectToContainer(new_form, extralist, 1, nullptr);

        return new_form->GetFormID();
    }

    FormID CreateFakeContainer(RE::TESBoundObject* container, RE::ExtraDataList* extralist) {
        std::string formtype(RE::FormTypeToString(container->GetFormType()));
        if (formtype == "SCRL") {return CreateFakeContainer<RE::ScrollItem>(container->As<RE::ScrollItem>(), extralist);}
        else if (formtype == "ARMO") {return CreateFakeContainer<RE::TESObjectARMO>(container->As<RE::TESObjectARMO>(), extralist);}
        else if (formtype == "BOOK") {return CreateFakeContainer<RE::TESObjectBOOK>(container->As<RE::TESObjectBOOK>(), extralist);}
        else if (formtype == "INGR") {return CreateFakeContainer<RE::IngredientItem>(container->As<RE::IngredientItem>(), extralist);}
        else if (formtype == "MISC") {return CreateFakeContainer<RE::TESObjectMISC>(container->As<RE::TESObjectMISC>(), extralist);}
        else if (formtype == "WEAP") {return CreateFakeContainer<RE::TESObjectWEAP>(container->As<RE::TESObjectWEAP>(), extralist);}
        else if (formtype == "AMMO") {return CreateFakeContainer<RE::TESAmmo>(container->As<RE::TESAmmo>(), extralist);}
        else if (formtype == "SLGM") {return CreateFakeContainer<RE::TESSoulGem>(container->As<RE::TESSoulGem>(), extralist);} 
        else if (formtype == "ALCH") {return CreateFakeContainer<RE::AlchemyItem>(container->As<RE::AlchemyItem>(), extralist);}
        else {
            RaiseMngrErr(std::format("Form type not supported: {}", formtype));
            return 0;
        }
    }

#define DISABLE_IF_NO_CURR_CONT if (!current_container) return RaiseMngrErr("Current container is null");

    RE::ObjectRefHandle RemoveItem(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                   RE::ITEM_REMOVE_REASON reason) { 
        auto ref_handle = RE::ObjectRefHandle();
        
        if (!moveFrom && !moveTo) {
            RaiseMngrErr("moveFrom and moveTo are both null!");
            return ref_handle;
        }
        listen_container_change = false;
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
				listen_container_change = true;
				return ref_handle;
			}
		}
        listen_container_change = true;
        return ref_handle;
    }

    RE::ObjectRefHandle RemoveItemReverse(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                   RE::ITEM_REMOVE_REASON reason) {
        auto ref_handle = RE::ObjectRefHandle();

        if (!moveFrom && !moveTo) {
            RaiseMngrErr("moveFrom and moveTo are both null!");
            return ref_handle;
        }
        logger::info("Removing item reverse");
        listen_container_change = false;
        auto inventory = moveFrom->GetInventory();
        for (auto item = inventory.rbegin(); item != inventory.rend(); ++item) {
            auto item_obj = item->first;
            if (!item_obj) RaiseMngrErr("Item object is null");
            if (item_obj->GetFormID() == item_id) {
                auto inv_data = item->second.second.get();
                if (!inv_data) RaiseMngrErr("Item data is null");
                auto asd = inv_data->extraLists;
                if (!asd || asd->empty()) {
                    ref_handle = moveFrom->RemoveItem(item_obj, 1, reason, nullptr, moveTo);
                } else {
                    ref_handle = moveFrom->RemoveItem(item_obj, 1, reason, asd->front(), moveTo);
                }
                listen_container_change = true;
                return ref_handle;
            }
        }
        listen_container_change = true;
        return ref_handle;
    }

    // Removes the object from the world and adds it to an inventory
    void RemoveObject(RE::TESObjectREFR* ref, RE::TESObjectREFR* move2container) {
        if (!move2container) return RaiseMngrErr("move2container is null or disabled or deleted");
        // Remove object from world
        if (!ref || ref->IsDisabled() || ref->IsDeleted()) return RaiseMngrErr("Object is null or disabled or deleted");
        
        ref->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));

        if (!PickUpItem(ref)) return RaiseMngrErr("RemoveObject:Failed to pick up item.");
        RemoveItemReverse(player_ref, move2container, ref->GetBaseObject()->GetFormID(),
                          RE::ITEM_REMOVE_REASON::kStoreInContainer);
    }

    void RemoveAllItemsFromChest(RE::TESObjectREFR* chest, bool transfer2player = true) {
        if (!chest) return RaiseMngrErr("Chest is null");
        auto move2ref = transfer2player ? player_ref : nullptr;
        listen_container_change = false;

        auto chest_container = chest->GetContainer();
        if (!chest_container) {
            logger::error("Chest container is null");
            Utilities::MsgBoxesNotifs::InGame::GeneralErr();
            return;
        }

        auto inventory = chest->GetInventory();
        for (const auto& item : inventory) {
            auto item_obj = item.first;
            if (!std::strlen(item_obj->GetName())) logger::warn("RemoveAllItemsFromChest: Item name is empty");
            auto item_count = item.second.first;
            auto inv_data = item.second.second.get();
            auto asd = inv_data->extraLists;
            if (!asd || asd->empty()) {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, move2ref);
            } else {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kRemove, asd->front(), move2ref);
            }
        }
        
        // need to handle if a fake container was inside this chest. yani cont_ref i bu cheste bakan data varsa
        // redirectlemeliyim
        for (auto& src : sources) {
            if (!Utilities::containsValue(src.data,chest->GetFormID())) continue;
            if (!transfer2player)
                RaiseMngrErr("Transfer2player is false, but a fake container was found in the chest.");
            for (const auto& [key, value] : src.data) {
                if (value == chest->GetFormID() && key!=value) {
                    logger::info(
                        "Fake container with formid {} found in chest during RemoveAllItemsFromChest. Redirecting...",
                        ChestToFakeContainer[key].innerKey);
                    src.data[key] = key;
                }
            }
        }

        listen_container_change = true;
    };

    // Activates a container
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
        if (!chest) return RaiseMngrErr("Chest is null");
        unownedChest->fullName = chest_name;
        Activate(chest);
    };

    void PromptInterface() {
        DISABLE_IF_NO_CURR_CONT
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
            [this](int result) { this->MsgBoxCallback(result); });
    }

    void MsgBoxCallback(int result) {
        DISABLE_IF_NO_CURR_CONT
        logger::info("Result: {}", result);

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

        // More
        if (result == 3) {
            return Utilities::MsgBoxesNotifs::ShowMessageBox(
                "...", buttons_more, [this](int res) { this->MsgBoxCallbackMore(res); });
        }

        // Cancel
        if (result == 2) return;
        
        // Take
        if (result == 1) {
            
            listen_activate = false;

            // Add fake container to player
            auto curr_container_base = current_container->GetBaseObject();
            auto chest = GetContainerChest(current_container);
            if (!chest) return RaiseMngrErr("Chest is null");
            auto chest_refid = chest->GetFormID();
            FormID fake_container_id;
            logger::info("Chest refid: {}", chest_refid);

            // if we already had created a fake container for this chest, then we just add it to the player's inventory
            if (!ChestToFakeContainer.count(chest_refid)) return RaiseMngrErr("Chest refid not found in ChestToFakeContainer, i.e. sth must have gone wrong during new form creation.");
            logger::info("Fake container formid found in ChestToFakeContainer");
            // it must be in the unownedchestOG
            if (!unownedChestOG) return RaiseMngrErr("unownedChestOG is null");
            // get the fake container from the unownedchestOG  and add it to the player's inventory
            fake_container_id = ChestToFakeContainer[chest_refid].innerKey;

            auto fake_refhandle = RemoveItemReverse(unownedChestOG, nullptr, fake_container_id, RE::ITEM_REMOVE_REASON::kDropping);
            if (!fake_refhandle.get().get()) return RaiseMngrErr("Failed to remove fake container from unownedChestOG");
            logger::info("Updating extras");
            UpdateExtras(current_container, fake_refhandle.get().get());
            // update weight and value
            auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_container_id);
            if (!fake_cont) return RaiseMngrErr("Fake container not found");
            UpdateFakeWV(fake_cont, chest);
            
            if (!PickUpItem(fake_refhandle.get().get())) return RaiseMngrErr("Failed to pick up fake container");

            // Update chest link (container is in inventory now so we replace the old refid with the chest refid -> {chestrefid:chestrefid})
            auto src = GetContainerSource(curr_container_base->GetFormID());
            if (!src) {return RaiseMngrErr("Could not find source for container");}
            src->data[chest_refid] = chest_refid;
            // Send the original container from the world to the linked chest
            RemoveObject(current_container, chest);



            if (_other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_container_id);


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

    void MsgBoxCallbackMore(int result) {
        logger::info("More");

        if (result != 0 && result != 1 && result != 2) return;

        // Cancel
        if (result == 2) return;

        // Back
        if (result == 1) return PromptInterface();

		//// Deregister
  //      if (result == 1) {
  //          if (!current_container) return RaiseMngrErr("current_container is null!");
  //          logger::info("Deregistering real container with name {} and form id {}", current_container->GetName(), current_container->GetBaseObject()->GetFormID());
  //          auto chest = GetContainerChest(current_container);
  //          DeRegisterChest(chest->GetFormID());
  //          logger::info("Deregistered source with name {} and form id {}", current_container->GetName(), current_container->GetBaseObject()->GetFormID());
  //          return;
		//}

        // Uninstall
        Uninstall();

    }
    
    // sadece fake containerin unownedChestOG'nin icinde olmasi gereken senaryolarda kullan. yani realcontainer out in the world iken.
    void HandleRegistration(RE::TESObjectREFR* a_container){

        // Create the fake container form and put in the unownedchestOG
        // extradata gets updates when the player picks up the real container and gets the fake container from
        // unownedchestOG

        if (!a_container) return RaiseMngrErr("Container is null");

        current_container = a_container;

        // get the source corresponding to the container that we are activating
        logger::info("Getting container source");
        auto src = GetContainerSource(a_container->GetBaseObject()->GetFormID());
        if (!src) return RaiseMngrErr("Could not find source for container");

        // register the container to the source data if it is not registered
        const auto container_refid = a_container->GetFormID();
        if (!container_refid) return RaiseMngrErr("Container refid is null");
        if (!Utilities::containsValue(src->data,container_refid)) {
            // Not registered. lets find a chest to register it to
            logger::info("Not registered");
            const auto ChestObjRef = FindNotMatchedChest();
            const auto ChestRefID = ChestObjRef->GetFormID();
            logger::info("Matched chest with refid: {} with container with refid: {}", ChestRefID, container_refid);
            if (!src->data.insert({ChestRefID, container_refid}).second) {
                return RaiseMngrErr(
                    std::format("Failed to insert chest refid {} and container refid {} into source data.", ChestRefID,
                                container_refid));
            }
            auto fake_formid = CreateFakeContainer(a_container->GetObjectReference(), nullptr);
            // add to ChestToFakeContainer
            if (!ChestToFakeContainer.insert({ChestRefID, {src->formid, fake_formid}})
                     .second) {
                return RaiseMngrErr("Failed to insert chest refid and fake container refid into ChestToFakeContainer.");
            }
        }
        // fake counterparti unownedchestOG de olmayabilir 
        // cunku load gameden sonra runtimeda halletmem gerekiyo. ekle
        else {
            bool already_in_unownedchest = true;
            auto inventory_chest_unownedOG = unownedChestOG->GetInventory();
            auto chest_ref = GetRealContainerChest(container_refid);
            // Check if fake container is in the chest
            auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_ref].innerKey);
            if (!fake_cont || inventory_chest_unownedOG.find(fake_cont) == inventory_chest_unownedOG.end()) {
                already_in_unownedchest = false;
            } 
            else if (!std::strlen(fake_cont->GetName())) {
                fake_cont->Copy(RE::TESForm::LookupByID<RE::TESForm>(src->formid));
                logger::info("Fake container found in unownedchestOG with name {} and formid {}.", fake_cont->GetName(),
                fake_cont->GetFormID());
            }
            if (!already_in_unownedchest) {
                logger::info("Fake container NOT found in unownedchestOG");
                auto real_container_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(src->formid);
                auto fakeid = CreateFakeContainer(real_container_obj, nullptr);
                // load game den dolayi
                logger::info("ChestToFakeContainer (chest refid: {}) before: {}", chest_ref,
                             ChestToFakeContainer[chest_ref].innerKey);
                ChestToFakeContainer[chest_ref].innerKey = fakeid;
                logger::info("ChestToFakeContainer (chest refid: {}) after: {}", chest_ref,
                    							 ChestToFakeContainer[chest_ref].innerKey);
            }
		}
    
    }

    bool IsFaved(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
        if (!item) {
            RaiseMngrErr("Item is null");
            return false;
        }
        if (!inventory_owner) {
            RaiseMngrErr("Inventory owner is null");
            return false;
        }
        auto inventory = inventory_owner->GetInventory();
        auto it = inventory.find(item);
        if (it != inventory.end()) return it->second.second->IsFavorited();
        return false;
    }

    bool IsFaved(RE::TESBoundObject* item) {return IsFaved(item, player_ref);}

    bool IsEquipped(RE::TESBoundObject* item) {
		auto inventory = player_ref->GetInventory();
		auto it = inventory.find(item);
		if (it != inventory.end()) return it->second.second->IsWorn();
        return false;
    }

    void FaveItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
		if (!item) return RaiseMngrErr("Item is null");
        if (!inventory_owner) return RaiseMngrErr("Inventory owner is null");
        auto inventory_changes = inventory_owner->GetInventoryChanges();
        auto entries = inventory_changes->entryList;
        for (auto it = entries->begin(); it != entries->end(); ++it) {
            auto formid = (*it)->object->GetFormID();
            if (!formid) logger::critical("Formid is null");
            if (formid == item->GetFormID()) {
                logger::info("Favoriting item: {}", item->GetName());
                bool no_extra_ = (*it)->extraLists->empty();
                logger::info("asdasd");
                if (no_extra_) {
                    logger::info("No extraLists");
                    inventory_changes->SetFavorite((*it), nullptr);
                } else {
                    logger::info("ExtraLists found");
                    inventory_changes->SetFavorite((*it), (*it)->extraLists->front());
                }
				return;
			}
        }
        logger::error("Item not found in inventory");
	}

    void FaveItem(RE::TESBoundObject* item) {
        FaveItem(item, player_ref);
    }

    void EquipItem(RE::TESBoundObject* item, bool unequip=false) {
        if (!item) return RaiseMngrErr("Item is null");
        auto inventory_changes = player_ref->GetInventoryChanges();
        auto entries = inventory_changes->entryList;
        for (auto it = entries->begin(); it != entries->end(); ++it) {
            auto formid = (*it)->object->GetFormID();
            if (formid == item->GetFormID()) {
                if (unequip) {
                    if ((*it)->extraLists->empty()) {
                        RE::ActorEquipManager::GetSingleton()->UnequipObject(player_ref->As<RE::Actor>(), (*it)->object, nullptr, 1,
                            (const RE::BGSEquipSlot*)nullptr, true, false, false);
                    } else {
                        RE::ActorEquipManager::GetSingleton()->UnequipObject(player_ref->As<RE::Actor>(), (*it)->object, (*it)->extraLists->front(), 1,
                            (const RE::BGSEquipSlot*)nullptr, true,false, false);
                    }
                } else {
                    if ((*it)->extraLists->empty()){
                        RE::ActorEquipManager::GetSingleton()->EquipObject(
                            player_ref->As<RE::Actor>(), (*it)->object, nullptr, 1,
                            (const RE::BGSEquipSlot*)nullptr, true, false, false, false);
                    } else{
                        RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref->As<RE::Actor>(), (*it)->object,
                                                                           (*it)->extraLists->front(), 1,
                            (const RE::BGSEquipSlot*)nullptr, true, false, false, false);
                    }
                }
                return;
            }
        }
    }

    [[nodiscard]] bool PickUpItem(RE::TESObjectREFR* item, unsigned int max_try = 3,
                    RE::Actor* actor = RE::PlayerCharacter::GetSingleton()->As<RE::Actor>()) {
        listen_container_change = false;
        RE::TESObjectREFR::InventoryItemMap actor_inventory;
        unsigned int i = 0;
        auto item_bound = item->GetBaseObject();
        if (!item_bound) {
            logger::warn("Item bound is null");
            return false;
        }
        while (i < max_try) {
            if (!item) {
                logger::warn("Item is null");
				return false;
            }
            if (!actor) {
                logger::warn("Actor is null");
                return false;
            }
			actor->PickUpObject(item, 1, false, false);
            actor_inventory = actor->GetInventory();
            if (actor_inventory.find(item_bound) != actor_inventory.end()) {
            	logger::info("Item picked up. Took {} tries.", i);
                listen_container_change = true;
                return true;
            }
			i++;
		}
        listen_container_change = true;
        return false;
    }

    void RemoveCarryWeightBoost(FormID item_formid){
        auto inventory_player = player_ref->GetInventory();
        auto item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid);
        auto enchantment = inventory_player.find(item_obj)->second.second->GetEnchantment();
        if (enchantment) {
            logger::info("Enchantment: {}", enchantment->GetName());
            // remove the enchantment from the fake container if it is carry weight boost
            for (const auto& effect : enchantment->effects) {
                logger::info("Effect: {}", effect->baseEffect->GetName());
                logger::info("PrimaryAV: {}", effect->baseEffect->data.primaryAV);
                logger::info("SecondaryAV: {}", effect->baseEffect->data.secondaryAV);
                if (effect->baseEffect->data.primaryAV == RE::ActorValue::kCarryWeight) {
                    logger::info("Removing enchantment: {}", effect->baseEffect->GetName());
                    if (effect->effectItem.magnitude > 0) effect->effectItem.magnitude = 0;
                }
            }
        }
	}

#undef DISABLE_IF_NO_CURR_CONT

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

        logger::info("No of chests in cell: {}", GetNoChests());

        // Check if unowned chest is in the cell and get its ref
        auto& runtimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : runtimeData.references) {
			if (!ref) continue;
			if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID() && !ChestToFakeContainer.count(ref->GetFormID())) {
				unownedChestOG = ref.get();
				break;
			}
		}

        if (!unownedChestOG || !unownedCell || !unownedChest || !unownedChest->As<RE::TESBoundObject>()) {
            logger::error("Failed to initialize Manager due to missing unowned chest/cell");
            init_failed = true;
        }

        if (init_failed) InitFailed();

        // Load also other settings...
        _other_settings = Settings::LoadOtherSettings();

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
            if (chest->GetInventory().find(fake_bound) != chest->GetInventory().end()) {
				RemoveItemReverse(chest, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
			}
			if (player_ref->GetInventory().find(fake_bound) != player_ref->GetInventory().end()) {
				RemoveItemReverse(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
			}
		}

        // Delete all unowned chests and try to return all items to the player's inventory while doing that
        auto& unownedRuntimeData = unownedCell->GetRuntimeData();
        for (const auto& ref : unownedRuntimeData.references) {
			if (!ref) continue;
            if (ref->GetFormID() == unownedChestOG->GetFormID()) continue;
			if (ref->GetBaseObject()->GetFormID() == unownedChest->GetFormID()) {
                if (ref->IsDisabled() && ref->IsDeleted()) continue;
				RemoveAllItemsFromChest(ref.get(), true);
                if (ref->GetInventory().size()) {
                    uninstall_successful = false;
                    logger::info("Chest with refid {} is not empty.", ref->GetFormID());
                    break;
                }
				ref->Disable();
				ref->SetDelete(true);
			}
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


        listen_activate = true; // this one stays
        listen_container_change = false;
        // set uninstalled flag to true
        isUninstalled = true;
    }

    void RaiseMngrErr(const std::string err_msg_ = "Error") {
        logger::error("{}", err_msg_);
        Utilities::MsgBoxesNotifs::InGame::CustomErrMsg(err_msg_);
        Utilities::MsgBoxesNotifs::InGame::GeneralErr();
        Uninstall();
    }

    void qTRICK__(Source& src, const SourceDataKey chest_ref, const SourceDataVal cont_ref, bool execute_trick = false) {
        
        logger::info("qTrick before execute_trick");
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
        auto chest_cont_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
        auto player_actor = player_ref->As<RE::Actor>();
        auto real_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(src.formid);

        if (!chest) return RaiseMngrErr("Chest not found");
        if (!chest_cont_ref) return RaiseMngrErr("Container chest not found");
        if (!player_actor) return RaiseMngrErr("Player actor is null");
        if (!real_bound) return RaiseMngrErr("Real bound not found");

        auto real_refhandle = RemoveItemReverse(chest, nullptr, src.formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (!real_refhandle) return RaiseMngrErr("Real refhandle is null.");
        // doing the trick for registering/creating the fake container
        if (execute_trick) {
            logger::info("Executing trick");
            src.data[chest_ref] = real_refhandle.get()->GetFormID();
            auto old_fakeid = ChestToFakeContainer[chest_ref].innerKey; // for external_favs
            HandleRegistration(real_refhandle.get().get());  // sets current_container, creates the fake with the
                                                             // chest link and puts it in unownedchestOG
            // if old_fakeid is in external_favs, we need to update it with new fakeid
            auto it = std::find(external_favs.begin(), external_favs.end(), old_fakeid);
            if (it != external_favs.end()) {
				external_favs.erase(it);
				external_favs.push_back(ChestToFakeContainer[chest_ref].innerKey);
			}
        }
        logger::info("qTrick after execute_trick");
        auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // the new one (execute_trick)
        auto chest_of_fake = execute_trick ? unownedChestOG : chest_cont_ref; // at the beginnning different, at the end same
        auto fake_refhandle = RemoveItemReverse(chest_of_fake, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (!fake_refhandle) return RaiseMngrErr("Fake refhandle is null.");
        /*auto fake_ref_native_handle = fake_refhandle.native_handle();
        if (!fake_ref_native_handle) return RaiseMngrErr("Fake ref native handle is null.");*/
        auto fake_ref = fake_refhandle.get().get();
        if (!fake_ref) return RaiseMngrErr("Fake ref is null.");
        logger::info("asdfasdfadsf");
        auto fake_bound = fake_ref->GetBaseObject();
        logger::info("asdfasdfadsf");
        if (!fake_bound) return RaiseMngrErr("Fake bound not found");
        if (fake_bound->GetFormID() != fake_formid) return RaiseMngrErr("Fake bound formid does not match");
        UpdateExtras(real_refhandle.get().get(), fake_ref);
        logger::info("Updating FakeWV");
        UpdateFakeWV(fake_bound, chest);
        
        logger::info("Picking up real");
        if (!PickUpItem(real_refhandle.get().get())) return RaiseMngrErr("Failed to pick up real container");
        logger::info("Picking up fake");
        if (!PickUpItem(fake_ref)) return RaiseMngrErr("Failed to pick up fake container");
        
        RemoveItemReverse(player_ref, chest, src.formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
        auto chest_inventory = chest->GetInventory();
        if (chest_inventory.find(real_bound) == chest_inventory.end()) return RaiseMngrErr("Real container not found in unownedchest's inventory");
        
        // fave it if it is in external_favs
        logger::info("Fave");
        auto it = std::find(external_favs.begin(), external_favs.end(), fake_formid);
        if (it != external_favs.end()) {
            logger::info("Faving");
            FaveItem(fake_bound);
        }
        // Remove carry weight boost if it has
        if (_other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_formid);
        // I do this bcs I changed it above
        if (execute_trick) src.data[chest_ref] = cont_ref;
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

    
    bool IsCONT(RefID refid) {
        return RE::FormTypeToString(
                   RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)->GetObjectReference()->GetFormType()) == "CONT";
    }

    // Checks if realcontainer_formid is in the sources
    bool IsRealContainer(const FormID formid) {
        for (const auto& src : sources) {
            if (src.formid == formid) return true;
        }
        return false;
    }

    // Checks if ref has formid in the sources
    bool IsRealContainer(const RE::TESObjectREFR* ref) {
        if (!ref) return false;
        if (ref->IsDisabled()) return false;
        if (ref->IsDeleted()) return false;
        auto base = ref->GetBaseObject();
        if (!base) return false;
        return IsRealContainer(base->GetFormID());
    }

    // if the src with this formid has some data, then we say it has registry
    bool RealContainerHasRegistry(const FormID realcontainer_formid) {
        for (const auto& src : sources) {
            if (src.formid == realcontainer_formid) {
                if (!src.data.empty()) return true;
                break;
            }
		}
		return false;
	}

    bool IsARegistry(const RefID registry) {
        for (const auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (cont_ref == registry) return true;
            }
		}
		return false;
	}

    // external refid is in one of the source data. unownedchests are allowed
    bool ExternalContainerIsRegistered(const RefID external_container_id) {
        if (!external_container_id) return false;
        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)->HasContainer()) {
            RaiseMngrErr("External container does not have a container.");
            return false;
        }
        return IsARegistry(external_container_id);
    }

    // external container can be found in the values of src.data
    bool ExternalContainerIsRegistered(const FormID fake_container_formid, const RefID external_container_id) {
        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_id)->HasContainer()) {
            RaiseMngrErr("External container does not have a container.");
            return false;
        }
        auto real_container_formid = FakeToRealContainer(fake_container_formid)->GetFormID();
        auto src = GetContainerSource(real_container_formid);
        if (!src) return false;
        return Utilities::containsValue(src->data,external_container_id);
    }

    bool IsFakeContainer(const FormID formid) {
        for (const auto& [chest_ref, cont_form] : ChestToFakeContainer) {
			if (cont_form.innerKey == formid) return true;
		}
		return false;
    }

    bool IsFakeContainer(const RE::TESObjectREFR* ref) {
        if (!ref) return false;
        if (ref->IsDisabled()) return false;
        if (ref->IsDeleted()) return false;
        auto base = ref->GetBaseObject();
        if (!base) return false;
        return IsFakeContainer(base->GetFormID());
    }

    bool IsUnownedChest(const RefID refid) {
        auto base = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)->GetBaseObject();
        return base->GetFormID() == unownedChest->GetFormID();
	}

    bool IsChest(const RefID chest_refid) { return ChestToFakeContainer.count(chest_refid) > 0; }

#define ENABLE_IF_NOT_UNINSTALLED if (isUninstalled) return;

    void UnHideReal(FormID fakeid) { 
        if (!hidden_real_ref) {
            return RaiseMngrErr("Hidden real ref is null");
        }
        
        auto fake_form = RE::TESForm::LookupByID(fakeid);
        if (fake_form->formFlags == 13) fake_form->formFlags = 9;

        auto chest_refid = GetFakeContainerChest(fakeid);
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        auto real_formid = ChestToFakeContainer[chest_refid].outerKey;
        if (hidden_real_ref->GetBaseObject()->GetFormID() != real_formid) {
            return RaiseMngrErr("Hidden real ref formid does not match the real formid");
        }
        player_ref->As<RE::Actor>()->PickUpObject(hidden_real_ref, 1, false, false);
        auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
        RemoveItemReverse(player_ref, chest, real_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
        hidden_real_ref = nullptr;
        UpdateFakeWV(fake_bound, chest);
    }

    bool SwapDroppedFakeContainer(RE::TESObjectREFR* ref_fake) {

        // need the linked chest for updating source data
        const auto chest_refid = GetFakeContainerChest(ref_fake->GetBaseObject()->GetFormID());
        const auto chestObjRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chestObjRef) {
			logger::error("chestObjRef is null");
            RaiseMngrErr("Chest not found.");
			return false;
		}

        auto real_base = FakeToRealContainer(ref_fake->GetBaseObject()->GetFormID());
        if (!real_base) {
            logger::info("real base is null");
            return false;
        }
        
        RE::ObjectRefHandle real_cont_handle = RemoveItemReverse(chestObjRef, nullptr, real_base->GetFormID(),
                                                RE::ITEM_REMOVE_REASON::kDropping);
        auto real_cont = real_cont_handle.get();
        real_cont->SetParentCell(ref_fake->GetParentCell());
        real_cont->SetPosition(ref_fake->GetPosition());
        
        RemoveObject(ref_fake,unownedChestOG);

        real_cont->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));

        // update source data
        auto src = GetContainerSource(real_base->GetFormID());
        if (!src) {
            RaiseMngrErr("Could not find source for container");
			return false;
		}
        src->data[chest_refid] = real_cont->GetFormID();

        logger::info("Swapped real container has refid: {}", real_cont->GetFormID());
        return true;
    }

    // need to put the real items in player's inventory and the fake items in unownedChestOG
    void HandleCraftingEnter() { 
        ENABLE_IF_NOT_UNINSTALLED
        listen_container_change = false;
        logger::info("Crafting menu opened");
        // this might be problematic since we dont update source data and chesttofakecontainer
        // trusting that the player will leave the crafting menu at some point and everything will be reverted
        for (const auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (chest_ref != cont_ref) continue;
                auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
                if (!chest) return RaiseMngrErr("Chest is null");
                RemoveItemReverse(chest, player_ref, src.formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                is_equipped[fake_formid] = IsEquipped(fake_bound);
                is_faved[fake_formid] = IsFaved(fake_bound);
                if (is_equipped[fake_formid]) EquipItem(fake_bound, true);
                RemoveItemReverse(player_ref, unownedChestOG, fake_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
            }
		}
        listen_container_change = true;
    }

    void HandleCraftingExit() { 
        ENABLE_IF_NOT_UNINSTALLED
        listen_container_change = false;
        logger::info("Crafting menu closed");
        for (auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (chest_ref != cont_ref) continue;
                auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
                if (!chest) return RaiseMngrErr("Chest is null");
                auto real_refhandle = RemoveItemReverse(player_ref, nullptr, src.formid, RE::ITEM_REMOVE_REASON::kDropping);
                if (!real_refhandle.get()) {
                    // it can happen when using arcane enchanter to destroy the item
                    logger::info("Real refhandle is null. Probably destroyed in arcane enchanter.");
                    DeRegisterChest(chest_ref);
                    continue;
                }
                auto fake_refhandle = RemoveItemReverse(unownedChestOG, nullptr, ChestToFakeContainer[chest_ref].innerKey,
                                      RE::ITEM_REMOVE_REASON::kDropping);
                UpdateExtras(real_refhandle.get().get(), fake_refhandle.get().get());
                player_ref->As<RE::Actor>()->PickUpObject(real_refhandle.get().get(), 1, false, false);
                RemoveItemReverse(player_ref, chest, src.formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
                UpdateFakeWV(fake_refhandle.get()->GetObjectReference(), chest);
                player_ref->As<RE::Actor>()->PickUpObject(fake_refhandle.get().get(), 1, false, false);
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                if (is_equipped[fake_formid]) EquipItem(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid));
                if (is_faved[fake_formid]) FaveItem(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid));
            }
        }
        is_equipped.clear();
        is_faved.clear();
        listen_container_change = true;
    }

    // places fake objects in external containers
    void HandleFakePlacement(RE::TESObjectREFR* external_cont) {
        ENABLE_IF_NOT_UNINSTALLED
        // if the external container is already handled (handled_external_containers) return
        //if (handled_external_conts.find(external_cont->GetFormID()) != handled_external_conts.end()) return;
        if (!external_cont) return RaiseMngrErr("external_cont is null");
        if (IsRealContainer(external_cont)) return;
        if (!external_cont->HasContainer()) return;
        if (!ExternalContainerIsRegistered(external_cont->GetFormID())) return;
        if (IsUnownedChest(external_cont->GetFormID())) return;


        listen_container_change = false;
		if (!external_cont) return RaiseMngrErr("external_cont is null");
        for (auto& src : sources) {
            if (!Utilities::containsValue(src.data,external_cont->GetFormID())) continue;
            for (const auto& [chest_ref, cont_ref] : src.data) {

                if (external_cont->GetFormID() != cont_ref) continue;
                // if the fake item is in it then continue
                auto inventory_external_cont = external_cont->GetInventory();
                auto fake_id = ChestToFakeContainer[chest_ref].innerKey;
                auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_id);
                if (inventory_external_cont.find(fake_bound) == inventory_external_cont.end()) {
                    qTRICK__(src, chest_ref, cont_ref, true);
                    RemoveItemReverse(player_ref, external_cont, ChestToFakeContainer[chest_ref].innerKey,
                                        RE::ITEM_REMOVE_REASON::kStoreInContainer);
                } 
                else if (!std::strlen(fake_bound->GetName())) {
					logger::info("Fake container found in external container but with empty name: {}", fake_bound->GetFormID());
                    fake_bound->Copy(RE::TESForm::LookupByID<RE::TESForm>(src.formid));
                    if (fake_id != fake_bound->GetFormID()) logger::error("Fake container formid changed from {} to {}", fake_id, fake_bound->GetFormID());
                    qTRICK__(src, chest_ref, cont_ref);
                    RemoveItemReverse(player_ref, external_cont, ChestToFakeContainer[chest_ref].innerKey,
                                      RE::ITEM_REMOVE_REASON::kStoreInContainer);
				}

            }
        }
        listen_container_change = true;
    }

    void HandleConsume(FormID fake_formid) {
        
        // check if player has the fake item
        auto player_inventory = player_ref->GetInventory();
        auto fake_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
        auto player_inventory_item = player_inventory.find(fake_obj);
        bool player_has_item = player_inventory_item != player_inventory.end();
        if (player_has_item && !player_inventory_item->second.first) {
            logger::info("Player has item but its count is zero.");
            // this is actually not necessary since DeRegisterChest will remove it from player
            RemoveItemReverse(player_ref, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
            logger::info("Item with zero count removed from player.");
            player_has_item = false;
        }
        if (player_has_item) return;
        
        // make sure unownedOG does not have it in inventory
        auto unownedchestOG_inventory = unownedChestOG->GetInventory();
        auto unownedchestOG_inventory_item = unownedchestOG_inventory.find(fake_obj);
        auto unownedchestOG_has_item = unownedchestOG_inventory_item != unownedchestOG_inventory.end();
        if (unownedchestOG_has_item) {
            logger::warn("UnownedchestOG has item.");
            return;
        }

        // make also sure that the real counterpart is still in unowned
        auto chest_ref = GetFakeContainerChest(fake_formid);
        auto chest_inventory = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref)->GetInventory();
        auto real_obj = FakeToRealContainer(fake_formid);
        auto chest_inventory_item = chest_inventory.find(real_obj);
        auto chest_has_item = chest_inventory_item != chest_inventory.end();
        if (!chest_has_item) return RaiseMngrErr("Real counterpart not found in unowned chest.");
        
        logger::info("Deregistering bcs Item consumed.");
        DeRegisterChest(chest_ref);
        // remove the real counterpart from player
        RemoveItemReverse(player_ref, nullptr, real_obj->GetFormID(), RE::ITEM_REMOVE_REASON::kRemove);
    }

    // Register an external container (technically could be another unownedchest of another of our containers) to the source data so that chestrefid of currentcontainer -> external container
    void LinkExternalContainer(const FormID fakecontainer, const RefID externalcontainer) {
        listen_container_change = false;

        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer)->HasContainer()) {
            return RaiseMngrErr("External container does not have a container.");
        }

        logger::info("Linking external container.");
        auto external_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer);
        auto realcontainer_boundobj = FakeToRealContainer(fakecontainer);
        auto src = GetContainerSource(realcontainer_boundobj->GetFormID());
        auto chest_refid = GetFakeContainerChest(fakecontainer);
        src->data[chest_refid] = externalcontainer;

        // if external container is one of ours (bcs of weight limit):
        if (IsChest(externalcontainer)) {
            logger::info("External container is one of our unowneds.");
            auto weight_limit = GetContainerSource(ChestToFakeContainer[externalcontainer].outerKey)->capacity;
            if (external_ref->GetWeightInContainer() > weight_limit) {
                RemoveItemReverse(external_ref, player_ref, fakecontainer,
                           RE::ITEM_REMOVE_REASON::kStoreInContainer);
                src->data[chest_refid] = chest_refid;
            }
        }

        // add it to handled_external_conts
        //handled_external_conts.insert(externalcontainer);

        // if successfully transferred to the external container, check if the fake container is faved
        if (src->data[chest_refid] != chest_refid &&
            IsFaved(RE::TESForm::LookupByID<RE::TESBoundObject>(fakecontainer), external_ref))
            logger::info("Faved item successfully transferred to external container.");
            external_favs.push_back(fakecontainer);


        listen_container_change = true;

    }

    void UnLinkExternalContainer(const FormID fake_container_formid, const RefID externalcontainer) { 

        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer)->HasContainer()) {
            return RaiseMngrErr("External container does not have a container.");
        }

        auto real_container_formid = FakeToRealContainer(fake_container_formid)->GetFormID();
        auto src = GetContainerSource(real_container_formid);
        auto chest_refid = GetFakeContainerChest(fake_container_formid);

        src->data[chest_refid] = chest_refid;

        // remove it from handled_external_conts
        //handled_external_conts.erase(externalcontainer);

        // remove it from external_favs
        auto it = std::find(external_favs.begin(), external_favs.end(), fake_container_formid);
        if (it != external_favs.end()) external_favs.erase(it);

        logger::info("Unlinked external container.");
    }

    void HandleSell(const FormID fake_container, const RefID sell_refid) {
        // assumes the sell_refid is a container
        ENABLE_IF_NOT_UNINSTALLED
        auto sell_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(sell_refid);
        logger::info("sell ref name: {}", sell_ref->GetName());
        if (!sell_ref) return RaiseMngrErr("Sell ref not found");
        // remove the fake container from vendor
        logger::info("Removed fake container from vendor");
        // add the real container to the vendor from the unownedchest
        auto chest_refid = GetFakeContainerChest(fake_container);
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chest) return RaiseMngrErr("Chest not found");
        RemoveItemReverse(sell_ref, nullptr, fake_container, RE::ITEM_REMOVE_REASON::kRemove);
        RemoveItemReverse(chest, sell_ref, FakeToRealContainer(fake_container)->GetFormID(),
                   RE::ITEM_REMOVE_REASON::kStoreInContainer);
        logger::info("Added real container to vendor chest");
        // remove all items from the chest to the player's inventory and deregister this chest
        DeRegisterChest(chest_refid);
        logger::info("Sell handled.");
    }

    void ActivateContainer(RE::TESObjectREFR* a_container) {
        ENABLE_IF_NOT_UNINSTALLED
        HandleRegistration(a_container);
        return PromptInterface();
    };

    void ActivateContainer(FormID fakeid, bool hide_real=false) {
        ENABLE_IF_NOT_UNINSTALLED
        auto chest_refid = GetFakeContainerChest(fakeid);
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        auto real_container_formid = FakeToRealContainer(fakeid)->GetFormID();
        auto real_container_name = RE::TESForm::LookupByID<RE::TESBoundObject>(real_container_formid)->GetName();
        if (hide_real) {
            // if the fake container was equipped, then we need to unequip it vice versa
            auto fake_form = RE::TESForm::LookupByID(fakeid);
            if (fake_form->formFlags != 13) fake_form->formFlags = 13;
			auto real_refhandle = RemoveItemReverse(chest, nullptr, real_container_formid, RE::ITEM_REMOVE_REASON::kDropping);
            hidden_real_ref = real_refhandle.get().get();
		}
        ActivateChest(chest, real_container_name);
    };

    void RevertEquip(FormID fakeid) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::info("RE::TESForm::LookupByID");
        auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
        logger::info("RE::TESForm::LookupByID___");
        auto unequip = IsEquipped(fake_bound);
        if (unequip) {
			EquipItem(fake_bound, true);
		} else {
			EquipItem(fake_bound);
		}
        /*auto inventory = player_ref->GetInventory();
        auto inventory_entry = inventory.find(fake_bound);
        logger::info("REsdfkjas");
        if (inventory_entry == inventory.end())
            RaiseMngrErr("Fake container not found in player's inventory");
        if (inventory_entry->second.second->IsWorn()) {
            logger::info("Fake container is equipped. Unequipping...");
            RE::ActorEquipManager::GetSingleton()->UnequipObject(player_ref->As<RE::Actor>(), fake_bound, nullptr, 1,
                                                                 nullptr, true, false, false);
        } else {
            logger::info("Fake container is not equipped. Equipping...");
            RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref->As<RE::Actor>(), fake_bound, nullptr, 1, nullptr, true, false, false,
                                                               false);
        }*/
    }

    // hopefully this works.
    void DropTake(FormID realcontainer_formid,uint32_t native_handle) { 
        // Assumes that the real container is in the player's inventory!

        logger::info("DropTaking...");

        if (!IsRealContainer(realcontainer_formid)) {
            logger::warn("Only real containers allowed");
        }
        if (!RealContainerHasRegistry(realcontainer_formid)) {
            logger::warn("Real container has no registry");
        }

        // need to do the trick?
        // RemoveItemReverse(player_ref, player_ref, realcontainer_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);

        // I need to update the source data.
        // To do that I will assume that the last added real container in player's inventory
        // is the one we are looking for
        // and still has existing reference...
        // I guess this happens when an item is added but the inventory is not updated yet
        auto src = GetContainerSource(realcontainer_formid);
        auto refhandle = RemoveItemReverse(player_ref, nullptr, realcontainer_formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (refhandle.get()->GetFormID() == native_handle) {
            logger::info("Native handle is the same as the refhandle refid");
        }
        else {
			logger::info("Native handle is NOT the same as the refhandle refid");
        }
        // lets make sure that the refid of the real container is still the same and exist in our data
        // if not I wouldnt know which fake form belongs to it?
  //      for (const auto& [chest_ref, cont_ref] : src->data) {
  //          if (cont_ref == refhandle.get()->GetFormID()) {
  //              ok = true;
		//		break;
		//	}
		//}
  //      if (!ok) return RaiseMngrErr("Real container is not registered");
        auto chest_refid = GetRealContainerChest(native_handle);
        src->data[chest_refid] = refhandle.get()->GetFormID();
        HandleRegistration(refhandle.get().get());
        MsgBoxCallback(1);
    }

    // uses current_container
    void InspectItemTransfer() {
        ENABLE_IF_NOT_UNINSTALLED
        if (!current_container) return RaiseMngrErr("current_container is null!");
        // check if container has enough capacity
        auto chest = GetContainerChest(current_container);
        auto weight_limit = GetContainerSource(current_container->GetBaseObject()->GetFormID())->capacity;
        while (chest->GetWeightInContainer() > weight_limit) {
            auto inventory = chest->GetInventory();
            auto item = inventory.rbegin();
            auto item_obj = item->first;
            auto item_count = item->second.first;
            auto inv_data = item->second.second.get();
            auto asd = inv_data->extraLists;
            listen_container_change = false;
            if (!asd || asd->empty()) {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, player_ref);
            } else {
                chest->RemoveItem(item_obj, item_count, RE::ITEM_REMOVE_REASON::kStoreInContainer, asd->front(),
                                  player_ref);
            }
            listen_container_change = true;
            RE::DebugNotification(std::format("{} is fully packed! Putting {} back.",
                                              current_container->GetDisplayFullName(), item_obj->GetName())
                                      .c_str());

        }
    }

    void Terminate() {
		ENABLE_IF_NOT_UNINSTALLED
		Uninstall();
	}

    void Reset() {
        ENABLE_IF_NOT_UNINSTALLED
        for (auto& src : sources) {
			src.data.clear();
		}
        ChestToFakeContainer.clear(); // we will update this in ReceiveData
        external_favs.clear(); // we will update this in ReceiveData
        Clear();
        //handled_external_conts.clear();
        current_container = nullptr;
        listen_menuclose = false;
        listen_activate = true;
        listen_container_change = true;
        isUninstalled = false;
        logger::info("Manager reset.");
    }

    void SendData() {
        ENABLE_IF_NOT_UNINSTALLED
        //std::lock_guard<std::mutex> lock(mutex);
        logger::info("--------Sending data---------");
        Print(); 
        Clear();
        for (auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                bool is_equipped_x = false;
                bool is_favorited_x = false;
                if (!chest_ref) return RaiseMngrErr("Chest refid is null");
                if (chest_ref == cont_ref) {
                    auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                    auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                    is_equipped_x = IsEquipped(fake_bound);
                    is_favorited_x = IsFaved(fake_bound);
                    /*auto inventory_changes = player_ref->GetInventoryChanges();
                    auto entries = inventory_changes->entryList;
                    for (auto it = entries->begin(); it != entries->end(); ++it) {
                        if ((*it)->object->GetFormID() == ChestToFakeContainer[chest_ref].innerKey) {
                            is_favorited = (*it)->IsFavorited();
                            is_equipped = (*it)->IsWorn();
                            break;
                        }
                    }*/
                } 
                // check if the fake container is faved in an external container
                else {
                    auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                    auto it = std::find(external_favs.begin(), external_favs.end(), fake_formid);
                    if (it != external_favs.end()) is_favorited_x = true;
                }
                FormIDX fake_container_x(ChestToFakeContainer[chest_ref].innerKey, is_equipped_x, is_favorited_x, "");
                SetData({src.formid, chest_ref}, {fake_container_x, cont_ref});
			}
        }
        logger::info("Data sent.");
    };

    void Something(Source& src, const RefID chest_ref) {
        const auto cont_ref = src.data[chest_ref];
        if (chest_ref == cont_ref) {
            logger::info("Player had fake item in inventory.");
            // in this case i need to place the fake container in player's inventory if it is not there
            // I have the fake_formid from ChestToFakeContainer
            bool already_in_inventory = true;
            auto fakeid = ChestToFakeContainer[chest_ref].innerKey;
            auto inventory_player = player_ref->GetInventory();
            // Check if fake container is in the chest
            auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
            // seems like between saves the fake container persists in the player's inventory but without any data
            if (!fake_cont || inventory_player.find(fake_cont) == inventory_player.end()) {
                already_in_inventory = false;
            } 
            else if (!std::strlen(fake_cont->GetName())) {
                fake_cont->Copy(RE::TESForm::LookupByID<RE::TESForm>(src.formid));
                if (fakeid != fake_cont->GetFormID()) logger::error("Fake container formid changed from {} to {}", fakeid, fake_cont->GetFormID());
                logger::info("Fake container found in player's inventory with name {} and formid {}.",
                             fake_cont->GetName(), fake_cont->GetFormID());
                qTRICK__(src, chest_ref, 0x14);
            }
            if (!already_in_inventory) {
                qTRICK__(src, chest_ref, cont_ref, true);
            }
        }
        // OR the external container is actually one of our chests! This can happen if a fake container is placed
        // "inside" another one
        else if (IsChest(cont_ref)) {
            logger::info("External container is one of our unowneds with refid: {}", cont_ref);
            bool already_in_chest = true;
            auto chest_cont_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
            if (!chest_cont_ref) return RaiseMngrErr("Chest not found");
            auto inventory_chest_cont_ref = chest_cont_ref->GetInventory();
            // Check if fake container is in the chest
            auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
            auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);

            if (!fake_cont || inventory_chest_cont_ref.find(fake_cont) == inventory_chest_cont_ref.end()) {
                already_in_chest = false;
            } else if (!std::strlen(fake_cont->GetName())) {
                fake_cont->Copy(RE::TESForm::LookupByID<RE::TESForm>(src.formid));
                qTRICK__(src, chest_ref, cont_ref);
                RemoveItemReverse(player_ref, chest_cont_ref, ChestToFakeContainer[chest_ref].innerKey,
                                  RE::ITEM_REMOVE_REASON::kStoreInContainer);
            }
            if (!already_in_chest) {
                logger::info("CONT REF BUT ACTUALLY ONE OF OUR CHESTS: {}", cont_ref);
                qTRICK__(src, chest_ref, cont_ref, true);
                // this is different than chestrefid == chestrefid case
                RemoveItemReverse(player_ref, chest_cont_ref, ChestToFakeContainer[chest_ref].innerKey,
                                  RE::ITEM_REMOVE_REASON::kStoreInContainer);
                // print the inventory of the chest
                inventory_chest_cont_ref = chest_cont_ref->GetInventory();
                for (const auto& [obj, count_entry] : inventory_chest_cont_ref) {
					logger::info("obj: {}, count: {}, formid: {}", obj->GetName(), count_entry.first, obj->GetFormID());
				}
                Print();
            } else {
                UpdateFakeWV(fake_cont, RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref));
            }
        }
    
    }

    void Something2(const RefID chest_ref, std::vector<RefID>& ha) {
        // ha: handled already
        if (std::find(ha.begin(), ha.end(), chest_ref) != ha.end()) return;
        logger::info("-------------------chest_ref: {} -------------------", chest_ref);
        auto connected_chests = ConnectedChests(chest_ref);
        for (const auto& connected_chest : connected_chests) {
            Something2(connected_chest,ha);
            //ha.push_back(connected_chest);
        }
        auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
        Something(*src, chest_ref);
        ha.push_back(chest_ref);
        logger::info("-------------------chest_ref: {} DONE -------------------", chest_ref);
        
    }

    void ReceiveData() {
        ENABLE_IF_NOT_UNINSTALLED
        logger::info("--------Receiving data---------");

        //std::lock_guard<std::mutex> lock(mutex);

        listen_container_change = false;
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
                if (realcontForm == src.formid) {
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
                    if (chestRef == contRef) chest_equipped_fav[chestRef] = {fakecontForm.equipped, fakecontForm.favorited};
                    else if (fakecontForm.favorited) external_favs.push_back(fakecontForm.id);
                    no_match = false;
                    break;
                }
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
            RemoveAllItemsFromChest(chest, true);
            // also remove the associated fake item from player or unowned chest
            auto fake_id = RealFakeForm_.innerKey;
            if (fake_id) {
                RemoveItemReverse(player_ref, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
                RemoveItemReverse(unownedChestOG, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kRemove);
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
			if (std::find(handled_already.begin(), handled_already.end(), chest_ref) != handled_already.end())
				continue;
            Something2(chest_ref,handled_already);
		}

        // print handled_already
        logger::info("handled_already: ");
        for (const auto& ref : handled_already) {
            logger::info("{}", ref);
        }


        handled_already.clear();

        // I make the fake containers in player inventory equipped/favorited:
        logger::info("Equipping and favoriting fake containers in player's inventory");
        auto inventory_changes = player_ref->GetInventoryChanges();
        auto entries = inventory_changes->entryList;
        for (auto it = entries->begin(); it != entries->end(); ++it){
            auto fake_formid = (*it)->object->GetFormID();
            if (IsFakeContainer(fake_formid)) {
                bool is_equipped_x = chest_equipped_fav[GetFakeContainerChest(fake_formid)].first;
                bool is_faved_x = chest_equipped_fav[GetFakeContainerChest((*it)->object->GetFormID())].second;
                if (is_equipped_x) {
                    logger::info("Equipping fake container with formid {}", fake_formid);
                    EquipItem((*it)->object);
                    /*RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref->As<RE::Actor>(), (*it)->object, 
                        nullptr,1,(const RE::BGSEquipSlot*)nullptr,true,false,false,false);*/
				}
                if (is_faved_x) {
                    logger::info("Favoriting fake container with formid {}", fake_formid);
                    FaveItem((*it)->object);
                    //inventory_changes->SetFavorite((*it), (*it)->extraLists->front());
                }
			}
        }

        logger::info("--------Receiving data done---------");
        Print();
        

        // At the very end check for nonexisting fakeforms from ChestToFakeContainer
        /*RE::TESBoundObject* fake_cont_;
        for (auto it = ChestToFakeContainer.begin(); it != ChestToFakeContainer.end();++it) {
            fake_cont_ = RE::TESForm::LookupByID<RE::TESBoundObject>(it->second.innerKey);
            if (!fake_cont_) logger::error("Fake container not found with formid {}", it->second.innerKey);
            else if (!std::strlen(fake_cont_->GetName()))
                logger::error("Fake container with formid {} does not have name.", it->second.innerKey);
		}*/
        
        current_container = nullptr;
        listen_container_change = true;
    };

    std::vector<RefID> ConnectedChests(const RefID chestRef) {
		std::vector<RefID> connected_chests;
        for (const auto& [chest_ref, cont_ref] : ChestToFakeContainer) {
            auto src = GetContainerSource(cont_ref.outerKey);
            if (chestRef != chest_ref && src->data[chest_ref] == chestRef)
			    connected_chests.push_back(chest_ref);
		}
		return connected_chests;
	}

    void Print() {
        
        for (const auto& src : sources) { 
            if (!src.data.empty()) {
                logger::info("Printing............Source formid: {}", src.formid);
                Utilities::printMap(src.data); 
            }
        }
        for (const auto& [chest_ref, cont_ref] : ChestToFakeContainer) {
			logger::info("Chest refid: {}, Real container formid: {}, Fake container formid: {}", chest_ref, cont_ref.outerKey, cont_ref.innerKey);
        }
        //Utilities::printMap(ChestToFakeContainer);
    }

#undef ENABLE_IF_NOT_UNINSTALLED

};
