#pragma once

#include "Settings.h"

using namespace Utilities::FunctionsSkyrim;


// TODO: handled_external_conts duzgun calisiyo mu test et. save et. barrela fakeplacement yap. save et. roll back save. sonra ilerki save e geri don, bakalim fakeitemlar hala duruyor mu

class Manager : public Utilities::SaveLoadData {
    // private variables
    const std::vector<std::string> buttons = {"Open", "Take", "More...", "Close"};
    const std::vector<std::string> buttons_more = {"Rename", "Uninstall", "Back", "Close"};
    bool uiextensions_is_present = false;
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
    std::vector<RefID> handled_external_conts; // runtime specific to prevent unnecessary checks in HandleFakePlacement
    std::map<FormID,std::string> renames;  // runtime specific, custom names for fake containers


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

    // returns items in chest to player and removes entries linked to the chest from src.data and ChestToFakeContainer
    void DeRegisterChest(const RefID chest_ref) {
        logger::info("Deregistering chest");
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
        if (!chest) return RaiseMngrErr("Chest not found");
        auto src = GetContainerSource(ChestToFakeContainer[chest_ref].outerKey);
        if (!src) return RaiseMngrErr("Source not found");
        RemoveAllItemsFromChest(chest, player_ref);
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

    void UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to) {
        logger::trace("UpdateExtras");
        if (!copy_from || !copy_to) return RaiseMngrErr("copy_from or copy_to is null");
        // Enchantment
        if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
            logger::trace("Enchantment found");
            auto enchantment =
                static_cast<RE::ExtraEnchantment*>(copy_from->GetByType(RE::ExtraDataType::kEnchantment));
            if (enchantment) {
                RE::ExtraEnchantment* enchantment_fake = RE::BSExtraData::Create<RE::ExtraEnchantment>();
                // log the associated actor value
                logger::trace("Associated actor value: {}", enchantment->enchantment->GetAssociatedSkill());
                enchantment_fake->enchantment = enchantment->enchantment;
                enchantment_fake->charge = enchantment->charge;
                enchantment_fake->removeOnUnequip = enchantment->removeOnUnequip;
                copy_to->Add(enchantment_fake);
            } else
                RaiseMngrErr("Failed to get enchantment from copy_from");
        }
        // Health
        if (copy_from->HasType(RE::ExtraDataType::kHealth)) {
            logger::trace("Health found");
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
            logger::trace("Rank found");
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
            logger::trace("TimeLeft found");
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
            logger::trace("Charge found");
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
            logger::trace("Scale found");
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
            logger::trace("UniqueID found");
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
            logger::trace("Poison found");
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
            logger::trace("ObjectHealth found");
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
            logger::trace("Light found");
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
            logger::trace("Radius found");
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
            logger::trace("Sound found");
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
            logger::trace("LinkedRef found");
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
            logger::trace("Horse found");
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
            logger::trace("Hotkey found");
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
            logger::trace("WeaponAttackSound found");
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
            logger::trace("ActivateRef found");
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
            logger::trace("TextDisplayData found");
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
            logger::trace("Soul found");
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
            logger::trace("Flags found");
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
            logger::trace("Lock found");
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
            logger::trace("Teleport found");
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
            logger::trace("LockList found");
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
            logger::trace("OutfitItem found");
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
            logger::trace("CannotWear found");
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
            logger::trace("Ownership found");
            auto ownership = static_cast<RE::ExtraOwnership*>(copy_from->GetByType(RE::ExtraDataType::kOwnership));
            if (ownership) {
                logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
                RE::ExtraOwnership* ownership_fake = RE::BSExtraData::Create<RE::ExtraOwnership>();
                ownership_fake->owner = ownership->owner;
                copy_to->Add(ownership_fake);
                logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
            } else
                RaiseMngrErr("Failed to get ownership from copy_from");
        }
    }

    void UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to) {
        logger::trace("UpdateExtras");
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
    void UpdateFakeWV(T* fake_form, RE::TESObjectREFR* chest_linked, const unsigned int value_offset) {
        logger::trace("UpdateFakeWV");
        
        // assumes base container is already in the chest
        if (!chest_linked || !fake_form) return RaiseMngrErr("Failed to get chest.");
        auto real_container = FakeToRealContainer(fake_form->GetFormID());
        logger::trace("Copying from real container to fake container. Real container: {}, Fake container: {}",
					 real_container->GetFormID(), fake_form->GetFormID());
        fake_form->Copy(real_container->As<T>());
        // if it was renamed, rename it back
        if (renames.count(fake_form->GetFormID())) fake_form->fullName = renames[fake_form->GetFormID()];

        FormTraits<T>::SetWeight(fake_form, chest_linked->GetWeightInContainer());

        int value_ = FormTraits<T>::GetValue(real_container->As<T>());
        if (_other_settings[Settings::otherstuffKeys[3]]) {
            logger::trace("VALUE BEFORE {}", value_);
            logger::trace("VALUE OFFSET {}", value_offset);
            value_ += GetValueInContainer(chest_linked) - value_offset;
            logger::trace("VALUE AFTER {}",value_);
        }
        if (value_ < 0) value_ = 0;

        logger::trace("Setting weight and value for fake form");
        FormTraits<T>::SetValue(fake_form, value_);
        logger::trace("ACTUAL VALUE {}", FormTraits<T>::GetValue(fake_form));
        
        if (!HasItem(fake_form, player_ref) || value_ == 0) return;
        // if the fake is in player inventory, we try to adjust the value to be correct
        // we also need to have that the actual value in the inventory can only be larger than we we have set
        logger::trace("Player has the fake form, try to correct the value");
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_form->GetFormID());
        int value_in_inventory = GetItemValue(fake_bound,player_ref);
        const int original_value_in_inventory = value_in_inventory; // value of fake item in the inventory after the first SetValue
        const int target_value = GetValueInContainer(chest_linked);
        if (original_value_in_inventory <= target_value) return;

        // do binary search to find the correct value up to a tolerance
        const float tolerance = 0.005f; // 0.5%
        const float tolerance_val = std::max(5.0f, std::floor(tolerance * target_value) + 1);  // at least 5 
        int max_iter = 2000;
        const int max_iter_ = max_iter;
        int value__ = value_;
        // value_in_inventory needs to be equal to value_ within tolerance_val
        logger::trace("Value in inventory: {}", value_in_inventory);
        logger::trace("value_: {}", value_);

        while (std::abs(value_in_inventory - target_value) > tolerance_val && max_iter > 0) {
            if (value_in_inventory > target_value) {
                value__ /= 2;
			} else {
                value__ += value__/ 2;
			}
            if (value__ < 0) return FormTraits<T>::SetValue(fake_form, value_);
            FormTraits<T>::SetValue(fake_form, value__);
            value_in_inventory = GetItemValue(fake_bound, player_ref);
            if (value_in_inventory < value__) return FormTraits<T>::SetValue(fake_form, value_);
			max_iter--;
		}

        logger::trace("iter: {}", max_iter_ - max_iter);

        if (max_iter == 0) {
            logger::warn("Max iterations reached.");
            if (std::abs(value_in_inventory - target_value) > std::abs(original_value_in_inventory - target_value)){
                logger::warn("Could not find a better value for fake form");
                return FormTraits<T>::SetValue(fake_form, value_);
            }
        } 
    }

    // Updates weight and value of fake container and uses Copy and applies renaming
    void UpdateFakeWV(RE::TESBoundObject* fake_form, RE::TESObjectREFR* chest_linked, const unsigned int value_offset) {
        logger::trace("pre-UpdateFakeWV");
        std::string formtype(RE::FormTypeToString(fake_form->GetFormType()));
        if (formtype == "SCRL") UpdateFakeWV<RE::ScrollItem>(fake_form->As<RE::ScrollItem>(), chest_linked,value_offset);
        else if (formtype == "ARMO") UpdateFakeWV<RE::TESObjectARMO>(fake_form->As<RE::TESObjectARMO>(), chest_linked,value_offset);
        else if (formtype == "BOOK") UpdateFakeWV<RE::TESObjectBOOK>(fake_form->As<RE::TESObjectBOOK>(), chest_linked,value_offset);
        else if (formtype == "INGR") UpdateFakeWV<RE::IngredientItem>(fake_form->As<RE::IngredientItem>(), chest_linked,value_offset);
        else if (formtype == "MISC") UpdateFakeWV<RE::TESObjectMISC>(fake_form->As<RE::TESObjectMISC>(), chest_linked,value_offset);
        else if (formtype == "WEAP") UpdateFakeWV<RE::TESObjectWEAP>(fake_form->As<RE::TESObjectWEAP>(), chest_linked,value_offset);
        else if (formtype == "AMMO") UpdateFakeWV<RE::TESAmmo>(fake_form->As<RE::TESAmmo>(), chest_linked,value_offset);
        else if (formtype == "SLGM") UpdateFakeWV<RE::TESSoulGem>(fake_form->As<RE::TESSoulGem>(), chest_linked,value_offset);
        else if (formtype == "ALCH") UpdateFakeWV<RE::AlchemyItem>(fake_form->As<RE::AlchemyItem>(), chest_linked,value_offset);
        else RaiseMngrErr(std::format("Form type not supported: {}", formtype));
        
    }

    template <typename T>
    const FormID CreateFakeContainer(T* realcontainer, RE::ExtraDataList* extralist) {
        logger::trace("CreateFakeContainer");
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

    // Creates new form for fake container and adds it to unownedChestOG
    const FormID CreateFakeContainer(RE::TESBoundObject* container, RE::ExtraDataList* extralist) {
        logger::trace("pre-CreateFakeContainer");
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

    template <typename T>
    void Rename(const std::string new_name, T item) {
        logger::trace("Rename");
        if (!item) logger::warn("Item not found");
        else item->fullName = new_name;
    }

    // takes the fake container from wherever it was stored (chest_of_fake) and puts it in the player's inventory.
    // also updates it based on its linked chest (chest_linked)
    // real container counterpart will be fetched from linked chest if not provided
    // it will be sent back to the linked chest at the end
    // DOES NOT UPDATE THE SOURCE OR ChestToFakeContainer !!!
    void FetchStoredFake(RE::TESObjectREFR* chest_of_fake, const FormID fake_id, const RefID chest_linked,RE::TESObjectREFR* real_ref=nullptr) {

        logger::trace("FetchStoredFake");
        
        auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_linked);
        if (!chest) return RaiseMngrErr("Chest linked is null");
        
        auto fake_refhandle = RemoveItemReverse(chest_of_fake, nullptr, fake_id, RE::ITEM_REMOVE_REASON::kDropping);
        if (!fake_refhandle) return RaiseMngrErr("Fake refhandle is null.");
        /*auto fake_ref_native_handle = fake_refhandle.native_handle();
        if (!fake_ref_native_handle) return RaiseMngrErr("Fake ref native handle is null.");*/
        auto fake_ref = fake_refhandle.get().get();
        if (!fake_ref) return RaiseMngrErr("Fake ref is null.");
        auto fake_bound = fake_ref->GetBaseObject();
        if (!fake_bound) return RaiseMngrErr("Fake bound not found");
        if (fake_bound->GetFormID() != fake_id) return RaiseMngrErr("Fake bound formid does not match");
        
        if (!real_ref) {
            auto real_formid = ChestToFakeContainer[chest_linked].outerKey;
            auto real_refhandle = RemoveItemReverse(chest, nullptr, real_formid, RE::ITEM_REMOVE_REASON::kDropping);
            if (!real_refhandle) return RaiseMngrErr("FetchStoredFake: Real refhandle is null.");
            real_ref = real_refhandle.get().get();
            if (!real_ref) return RaiseMngrErr("Real ref is null.");
		}

        // Updates
        UpdateExtras(real_ref, fake_ref);
        int value_offset = GetValueInContainer(chest);

        // realla isimiz bitti
        logger::trace("Sending dropped real in unownedcell to its unownedchest (chest_linked)");
        if (!RemoveObject(real_ref, chest, false)) return RaiseMngrErr("Failed to send dropped real in unownedcell to its unownedchest");
        value_offset = GetValueInContainer(chest) - value_offset;
        if (value_offset < 0) {
            logger::error("Value offset is negative. Setting it to 0");
			value_offset = 0;
        }
        logger::trace("Picking up fake");
        if (!PickUpItem(fake_ref)) return RaiseMngrErr("Failed to pick up fake container");

        logger::trace("Updating FakeWV");
        UpdateFakeWV(fake_bound,chest,value_offset);

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

    // removes only one unit of the item
    const RE::ObjectRefHandle RemoveItemReverse(RE::TESObjectREFR* moveFrom, RE::TESObjectREFR* moveTo, FormID item_id,
                                   RE::ITEM_REMOVE_REASON reason) {

        logger::trace("RemoveItemReverse");

        auto ref_handle = RE::ObjectRefHandle();

        if (!moveFrom && !moveTo) {
            RaiseMngrErr("moveFrom and moveTo are both null!");
            return ref_handle;
        }
        logger::trace("Removing item reverse");
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
        
        if (owned) ref->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
        if (!PickUpItem(ref)) {
            logger::error("Failed to pick up item");
            return false;
        }
        const auto ref_bound = ref->GetBaseObject();
        RemoveItemReverse(player_ref, move2container, ref_bound->GetFormID(),
                          RE::ITEM_REMOVE_REASON::kStoreInContainer);
        if (!HasItem(ref_bound, move2container)) {
            logger::error("Real container not found in move2container");
            return false;
        }
        
        return true;
    }

    void RemoveAllItemsFromChest(RE::TESObjectREFR* chest, RE::TESObjectREFR* move2ref = nullptr) {

        logger::trace("RemoveAllItemsFromChest");

        if (!chest) return RaiseMngrErr("Chest is null");
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
            if (!Utilities::Functions::containsValue(src.data,chest->GetFormID())) continue;
            if (!move2ref) RaiseMngrErr("move2ref is null, but a fake container was found in the chest.");
            for (const auto& [key, value] : src.data) {
                if (value == chest->GetFormID() && key!=value) {
                    logger::info(
                        "Fake container with formid {} found in chest during RemoveAllItemsFromChest. Redirecting...",
                        ChestToFakeContainer[key].innerKey);
                    if (move2ref->IsPlayerRef()) src.data[key] = key;
                    // must be sell_ref
                    else {
                        // the chest that is connected to the fake container which was inside this chest
                        HandleSell(ChestToFakeContainer[key].innerKey, move2ref->GetFormID());
                        listen_container_change = false;
                    }
                }
            }
        }

        listen_container_change = true;
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
        listen_menuclose = true;
        unownedChest->fullName = chest_name;
        logger::trace("Activating chest with name: {}", chest_name);
        logger::trace("listenclose: {}", listen_menuclose);
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

    void MsgBoxCallback(const int result) {
        DISABLE_IF_NO_CURR_CONT
        logger::trace("Result: {}", result);

        if (result != 0 && result != 1 && result != 2 && result != 3) return;

        // More
        if (result == 2) {
            return Utilities::MsgBoxesNotifs::ShowMessageBox("...", buttons_more,
                                                             [this](const int res) { this->MsgBoxCallbackMore(res); });
        }

        // Close
        if (result == 3) return;
        
        // Take
        if (result == 1) {
            
            listen_activate = false;

            
            // Add fake container to player
            const auto chest_refid = GetRealContainerChest(current_container->GetFormID());
            logger::trace("Chest refid: {}", chest_refid);
            // if we already had created a fake container for this chest, then we just add it to the player's inventory
            if (!ChestToFakeContainer.count(chest_refid)) return RaiseMngrErr("Chest refid not found in ChestToFakeContainer, i.e. sth must have gone wrong during new form creation.");
            logger::trace("Fake container formid found in ChestToFakeContainer");
            
            // get the fake container from the unownedchestOG  and add it to the player's inventory
            const FormID fake_container_id = ChestToFakeContainer[chest_refid].innerKey;
            if (!unownedChestOG) return RaiseMngrErr("unownedChestOG is null");
            // fake must be in the unownedchestOG
            if (!HasItem(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_container_id), unownedChestOG)) return RaiseMngrErr("Fake container not found in unownedChestOG");
            FetchStoredFake(unownedChestOG,fake_container_id,chest_refid,current_container);
            
            // Update chest link (fake container is in inventory now so we replace the old refid with the chest refid -> {chestrefid:chestrefid})
            auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
            if (!src) {return RaiseMngrErr("Could not find source for container");}
            src->data[chest_refid] = chest_refid;

            if (_other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_container_id);

            current_container = nullptr;
            listen_activate = true;

            return;
        }

        // Opening container

        // Listen for menu close
        //listen_menuclose = true;

        // Activate the unowned chest
        auto chest = GetRealContainerChest(current_container);
        auto fake_id = ChestToFakeContainer[chest->GetFormID()].innerKey;
        auto chest_rename = renames.count(fake_id) ? renames[fake_id].c_str() : current_container->GetName();
        ActivateChest(chest, chest_rename);
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
                    if (vm->DispatchStaticCall("UIExtensions", "OpenMenu", args, callback)) listen_menuclose = true;
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

    // sadece fake containerin unownedChestOG'nin icinde olmasi gereken senaryolarda kullan. yani realcontainer out in the world iken.
    void HandleRegistration(RE::TESObjectREFR* a_container){
        // Create the fake container form and put in the unownedchestOG
        // extradata gets updates when the player picks up the real container and gets the fake container from
        // unownedchestOG

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
            const auto chest_refid = GetRealContainerChest(container_refid);
            // Check if fake container is in unownedchestOG
            const auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(ChestToFakeContainer[chest_refid].innerKey);
            // bi de bazen deleted oluyo fake container droplarken load gameden sonra. ona refresh cakmamizi sagliyo bu.
            if (!fake_cont || !HasItemPlusCleanUp(fake_cont, unownedChestOG)) {
                logger::info("Fake container NOT found in unownedchestOG");
                const auto real_container_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(src->formid);
                const auto fakeid = CreateFakeContainer(real_container_obj, nullptr);
                // load game den dolayi
                logger::trace("ChestToFakeContainer (chest refid: {}) before: {}", chest_refid,
                             ChestToFakeContainer[chest_refid].innerKey);
                ChestToFakeContainer[chest_refid].innerKey = fakeid;
                logger::trace("ChestToFakeContainer (chest refid: {}) after: {}", chest_refid,
                             ChestToFakeContainer[chest_refid].innerKey);
            }
            // we dont care about updating other stuff at this stage since we will do it in "Take" button
            // we just need to make sure that the fake container is in unownedchestOG
            else if (!std::strlen(fake_cont->GetName())) {
                fake_cont->Copy(RE::TESForm::LookupByID<RE::TESForm>(src->formid));
                logger::info("Fake container found in unownedchestOG with name {} and formid {}.", fake_cont->GetName(),
                fake_cont->GetFormID());
            }
		}
    
    }

    [[nodiscard]] const bool IsFaved(RE::TESBoundObject* item) { return IsFavorited(item, player_ref); }

    [[nodiscard]] const bool IsEquipped(RE::TESBoundObject* item) {

        logger::trace("IsEquipped");

        if (!item) {
            logger::trace("Item is null");
            return false;
        }
		auto inventory = player_ref->GetInventory();
		auto it = inventory.find(item);
        if (it != inventory.end()) {
            if (it->second.first <= 0) logger::warn("Item count is 0");
            return it->second.second->IsWorn();
        }
        return false;
    }

    void FaveItem(RE::TESBoundObject* item) { FavoriteItem(item, player_ref);}

    void FaveItem(const FormID formid) { FaveItem(RE::TESForm::LookupByID<RE::TESBoundObject>(formid));}

    void EquipItem(RE::TESBoundObject* item, bool unequip=false) {

        logger::trace("EquipItem");

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

    [[nodiscard]] const bool PickUpItem(RE::TESObjectREFR* item, const unsigned int max_try = 3,
                    RE::Actor* actor = RE::PlayerCharacter::GetSingleton()->As<RE::Actor>()) {
        logger::trace("PickUpItem");
        //std::lock_guard<std::mutex> lock(mutex);
        listen_container_change = false;
        auto item_bound = item->GetBaseObject();
        const auto item_count = GetItemCount(item_bound, actor);
        logger::trace("Item count: {}", item_count);
        unsigned int i = 0;
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
            logger::trace("Critical: PickUpItem");
			actor->PickUpObject(item, 1, false, false);
            logger::trace("Item picked up. Checking if it is in inventory...");
            if (GetItemCount(item_bound, actor) > item_count) {
            	logger::trace("Item picked up. Took {} extra tries.", i);
                listen_container_change = true;
                return true;
            }
            else logger::trace("item count: {}", GetItemCount(item_bound, actor));
			i++;
		}
        listen_container_change = true;
        return false;
    }

    void RemoveCarryWeightBoost(const FormID item_formid){

        logger::trace("RemoveCarryWeightBoost");

        auto item_obj = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid);
        if (!item_obj) return RaiseMngrErr("Item not found");
        if (!HasItem(item_obj, player_ref)) {
            logger::warn("Item not found in player's inventory");
            return;
        }
        auto inventory_player = player_ref->GetInventory();
        if (auto enchantment = inventory_player.find(item_obj)->second.second->GetEnchantment()) {
            logger::trace("Enchantment: {}", enchantment->GetName());
            // remove the enchantment from the fake container if it is carry weight boost
            for (const auto& effect : enchantment->effects) {
                logger::trace("Effect: {}", effect->baseEffect->GetName());
                logger::trace("PrimaryAV: {}", effect->baseEffect->data.primaryAV);
                logger::trace("SecondaryAV: {}", effect->baseEffect->data.secondaryAV);
                if (effect->baseEffect->data.primaryAV == RE::ActorValue::kCarryWeight) {
                    logger::trace("Removing enchantment: {}", effect->baseEffect->GetName());
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
            auto form_ = GetFormByID(src.formid,src.editorid);
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
            while (HasItemPlusCleanUp(fake_bound, chest)) {
				RemoveItemReverse(chest, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
			}
            while (HasItemPlusCleanUp(fake_bound, player_ref)) {
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
                RemoveAllItemsFromChest(ref.get(), player_ref);
                if (ref->GetInventory().size()) {
                    uninstall_successful = false;
                    logger::error("Chest with refid {} is not empty.", ref->GetFormID());
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

    void TRICK(Source& src, const RE::ObjectRefHandle& real_handle, const RefID chest_refid) {

        logger::trace("TRICK");

        src.data[chest_refid] = real_handle.get()->GetFormID();
        auto old_fakeid = ChestToFakeContainer[chest_refid].innerKey;  // for external_favs
        HandleRegistration(real_handle.get().get());  // sets current_container, creates the fake with the
                                                         // chest link and puts it in unownedchestOG
        // if old_fakeid is in external_favs, we need to update it with new fakeid
        auto it = std::find(external_favs.begin(), external_favs.end(), old_fakeid);
        if (it != external_favs.end()) {
            external_favs.erase(it);
            external_favs.push_back(ChestToFakeContainer[chest_refid].innerKey);
        }
        // same goes for renames
        if (renames.count(old_fakeid) && ChestToFakeContainer[chest_refid].innerKey != old_fakeid) {
            renames[ChestToFakeContainer[chest_refid].innerKey] = renames[old_fakeid];
            renames.erase(old_fakeid);
        }
    }

    // for the cases when real container is in its chest and fake container is in some other inventory (player,unownedchest,external_container)
    // DOES NOT UPDATE THE SOURCE DATA and CHESTTOFAKECONTAINER !!!
    void qTRICK__(Source& src, const SourceDataKey chest_ref, const SourceDataVal cont_ref,bool execute_trick = false) {
        
        logger::trace("qTrick before execute_trick");
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
            logger::trace("Executing trick");
            TRICK(src, real_refhandle, chest_ref);
        }
        logger::trace("qTrick after execute_trick");
        auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // the new one (execute_trick)
        auto chest_of_fake = execute_trick ? unownedChestOG : chest_cont_ref; // at the beginnning different, at the end same

        FetchStoredFake(chest_of_fake, fake_formid, chest_ref, real_refhandle.get().get());

        // fave it if it is in external_favs
        logger::trace("Fave");
        auto it = std::find(external_favs.begin(), external_favs.end(), fake_formid);
        if (it != external_favs.end()) {
            logger::trace("Faving");
            FaveItem(fake_formid);
        }
        // Remove carry weight boost if it has
        if (_other_settings[Settings::otherstuffKeys[1]]) RemoveCarryWeightBoost(fake_formid);
        // I do this bcs I changed it in TRICK
        if (execute_trick) src.data[chest_ref] = cont_ref;
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

    //std::mutex mutex;

    
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
        logger::trace("IsFakeContainer");
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
        const auto base = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)->GetBaseObject();
        return base->GetFormID() == unownedChest->GetFormID();
	}

    // checks if the refid is in the ChestToFakeContainer, i.e. if it is an unownedchest
    [[nodiscard]] const bool IsChest(const RefID chest_refid) { return ChestToFakeContainer.count(chest_refid) > 0; }

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
        int value_offset = GetValueInContainer(chest);
        if (!RemoveObject(hidden_real_ref, chest)) return RaiseMngrErr("Failed to UnHideReal");
        value_offset = GetValueInContainer(chest) - value_offset;
        if (value_offset < 0) {
            logger::error("Value offset is negative. Value offset: {}", value_offset);
            value_offset = 0;
        }
        const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fakeid);
        hidden_real_ref = nullptr;
        UpdateFakeWV(fake_bound, chest, value_offset);
    }

    [[nodiscard]] const bool SwapDroppedFakeContainer(RE::TESObjectREFR* ref_fake) {

        logger::trace("SwapDroppedFakeContainer");

        if (!ref_fake) {
            logger::trace("Fake container is null.");
            return false;
        }
        
        // print gold value
        logger::trace("Gold value: {}", ref_fake->GetGoldValue());
        // print gold value with base object
        logger::trace("Gold value with base object: {}", ref_fake->GetBaseObject()->GetGoldValue());

        // need the linked chest for updating source data
        if (!ref_fake) {
			logger::warn("Fake container is null.");
			return false;
		}


        const auto chest_refid = GetFakeContainerChest(ref_fake->GetBaseObject()->GetFormID());
        const auto chestObjRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        if (!chestObjRef) {
			logger::error("chestObjRef is null");
            RaiseMngrErr("Chest not found.");
			return false;
		}

        const auto real_base = FakeToRealContainer(ref_fake->GetBaseObject()->GetFormID());
        if (!real_base) {
            logger::info("real base is null");
            return false;
        }
        
        const auto src = GetContainerSource(real_base->GetFormID());
        if (!src) {
            RaiseMngrErr("Could not find source for container");
			return false;
		}

        
        const RE::ObjectRefHandle real_cont_handle =
            RemoveItemReverse(chestObjRef, nullptr, real_base->GetFormID(),
                                                RE::ITEM_REMOVE_REASON::kDropping);
        if (!real_cont_handle) {
            logger::critical("Real container refhandle is null");
			return false;
        }
        const auto real_cont = real_cont_handle.get();
        real_cont->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
        /*real_cont->SetParentCell(ref_fake->GetParentCell());
        real_cont->SetPosition(ref_fake->GetPosition());*/
        //real_cont->SetMotionType(RE::TESObjectREFR::MotionType::kFixed);
        //real_cont->MoveTo(player_ref);
        //auto position = player_ref->GetLookingAtLocation();
        //logger::info("Looking at location: x: {}, y: {}, z: {}", position.x, position.y, position.z);
        
        // yes terrible naming scheme. i am tired
        const auto position = ref_fake->GetPosition();
        auto player_pos = player_ref->GetPosition();
        logger::trace("Player's position: x: {}, y: {}, z: {}", player_pos.x, player_pos.y, player_pos.z);
        // distance in the xy-plane
        const auto distance = std::sqrt(std::pow(position.x - player_pos.x, 2) + std::pow(position.y - player_pos.y, 2));
        if (distance < 60 || distance > 160) {
        	logger::info("Distance is not in the range of 60-160. Distance: {}", distance);
            const auto multiplier = 100.0f;
            player_pos += {multiplier, multiplier, 70};
        } else player_pos = position;
        // unit vector between the player and the looking at location
        //auto unit_vector = RE::NiPoint3{position.x - player_pos.x, position.y - player_pos.y, 0} / distance;
        // add it to the player's position with a distance of 100
        /*player_pos += {unit_vector.x * multiplier, unit_vector.y * multiplier, 70};*/
        logger::trace("New position: x: {}, y: {}, z: {}", player_pos.x, player_pos.y, player_pos.z);
        real_cont->SetParentCell(player_ref->GetParentCell());
        real_cont->SetPosition(player_pos);

        /*Player's position: x: -287.63037, y: -116.021225, z: 80.00003
        ref_fake's position: x: -190.57587, y: -136.17564, z: 151.63188 */


        //auto player_pos = player_ref->GetPosition();
        //logger::info("Player's position: x: {}, y: {}, z: {}", player_pos.x, player_pos.y, player_pos.z);
        //real_cont->SetParentCell(ref_fake->GetParentCell());
        //player_pos = ref_fake->GetPosition();
        //logger::info("ref_fake's position: x: {}, y: {}, z: {}", player_pos.x, player_pos.y, player_pos.z);
        //real_cont->SetPosition(ref_fake->GetPosition());


        // update source data
        src->data[chest_refid] = real_cont->GetFormID();

        if (ref_fake->IsDeleted()) {
            logger::warn("Fake container is deleted. GOnna try to create a new one while disabling this");
            ref_fake->Disable();
            ref_fake->SetDelete(true);
            TRICK(*src,real_cont_handle,chest_refid);

        } 
        else if (!RemoveObject(ref_fake, unownedChestOG)) {
            logger::error("Failed to remove dropped fake container to unownedChestOG");
            return false;
        }   

        logger::trace("Swapped real container has refid: {}", real_cont->GetFormID());
        return true;
    }

    // need to put the real items in player's inventory and the fake items in unownedChestOG
    void HandleCraftingEnter() { 
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("HandleCraftingEnter");
        listen_container_change = false;
        logger::trace("Crafting menu opened");
        // this might be problematic since we dont update source data and chesttofakecontainer
        // trusting that the player will leave the crafting menu at some point and everything will be reverted
        for (const auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (chest_ref != cont_ref) continue;
                const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_ref);
                if (!chest) return RaiseMngrErr("Chest is null");
                RemoveItemReverse(chest, player_ref, src.formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);
                const auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                const auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
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
        logger::trace("HandleCraftingExit");
        listen_container_change = false;
        logger::trace("Crafting menu closed");
        for (auto& src : sources) {
            for (const auto& [chest_ref, cont_ref] : src.data) {
                // we trust that the player will leave the crafting menu at some point and everything will be reverted
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
                
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                FetchStoredFake(unownedChestOG, fake_formid, chest_ref, real_refhandle.get().get());
                if (is_equipped[fake_formid]) EquipItem(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid));
                if (is_faved[fake_formid]) FaveItem(RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid));
            }
        }
        is_equipped.clear();
        is_faved.clear();
        listen_container_change = true;
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


        listen_container_change = false;
		if (!external_cont) return RaiseMngrErr("external_cont is null");
        for (auto& src : sources) {
            if (!Utilities::Functions::containsValue(src.data, external_cont->GetFormID())) continue;
            for (const auto& [chest_ref, cont_ref] : src.data) {
                if (external_cont->GetFormID() != cont_ref) continue;
                Something1(src, chest_ref, external_cont);
                // break yok cunku baska fakeler de external_cont un icinde olabilir
            }
        }
        handled_external_conts.push_back(external_cont->GetFormID());
        listen_container_change = true;
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
        if (HasItemPlusCleanUp(fake_obj, unownedChestOG)) { // false alarm???
            logger::warn("UnownedchestOG has item.");
            return;
        }

        // make also sure that the real counterpart is still in unowned
        const auto chest_refid = GetFakeContainerChest(fake_formid);
        const auto chest_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(chest_refid);
        const auto real_obj = FakeToRealContainer(fake_formid);
        if (!HasItem(real_obj, chest_ref)) return RaiseMngrErr("Real counterpart not found in unowned chest.");
        
        logger::info("Deregistering bcs Item consumed.");
        DeRegisterChest(chest_refid);
        // remove the real counterpart from player
        RemoveItemReverse(player_ref, nullptr, real_obj->GetFormID(), RE::ITEM_REMOVE_REASON::kRemove);
    }

    // Register an external container (technically could be another unownedchest of another of our containers) to the source data so that chestrefid of currentcontainer -> external container
    void LinkExternalContainer(const FormID fakecontainer, const RefID externalcontainer) {
        ENABLE_IF_NOT_UNINSTALLED

        listen_container_change = false;

        logger::trace("LinkExternalContainer");

        if (!RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer)->HasContainer()) {
            return RaiseMngrErr("External container does not have a container.");
        }

        logger::trace("Linking external container.");
        const auto external_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(externalcontainer);
        const auto chest_refid = GetFakeContainerChest(fakecontainer);
        const auto src = GetContainerSource(ChestToFakeContainer[chest_refid].outerKey);
        if (!src) return RaiseMngrErr("Source not found.");
        src->data[chest_refid] = externalcontainer;

        // if external container is one of ours (bcs of weight limit):
        if (IsChest(externalcontainer) && src->capacity > 0) {
            logger::info("External container is one of our unowneds.");
            const auto weight_limit = src->capacity;
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
            IsFavorited(RE::TESForm::LookupByID<RE::TESBoundObject>(fakecontainer), external_ref)) {
            logger::trace("Faved item successfully transferred to external container.");
            external_favs.push_back(fakecontainer);
        }


        listen_container_change = true;

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
        //handled_external_conts.erase(externalcontainer);

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
        else RemoveItemReverse(chest, sell_ref, FakeToRealContainer(fake_container)->GetFormID(),
                   RE::ITEM_REMOVE_REASON::kStoreInContainer);
        logger::trace("Added real container to vendor chest");
        // remove all items from the chest to the player's inventory and deregister this chest
        DeRegisterChest(chest_refid);
        logger::trace("Sell handled.");
    }

    void ActivateContainer(RE::TESObjectREFR* a_container) {
        ENABLE_IF_NOT_UNINSTALLED
        logger::trace("ActivateContainer 1 arg");
        HandleRegistration(a_container);
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
            logger::trace("Hiding real container");
            const auto fake_form = RE::TESForm::LookupByID(fakeid);
            if (fake_form->formFlags != 13) fake_form->formFlags = 13;
            const auto real_refhandle =
                RemoveItemReverse(chest, nullptr, real_container_formid, RE::ITEM_REMOVE_REASON::kDropping);
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

        // if reopeninitialmenu is true, then PromptInterface
        if (_other_settings[Settings::otherstuffKeys[2]]) PromptInterface();
    }

    // reverts inside the samw inventory
    void RevertEquip(const FormID fakeid) {
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

        // need to do the trick?
        // RemoveItemReverse(player_ref, player_ref, realcontainer_formid, RE::ITEM_REMOVE_REASON::kStoreInContainer);

        // I need to update the source data.
        // To do that I will assume that the last added real container in player's inventory
        // is the one we are looking for
        // and still has existing reference...
        // I guess this happens when an item is added but the inventory is not updated yet
        const auto src = GetContainerSource(realcontainer_formid);
        if (!src) return RaiseMngrErr("Could not find source for container");
        const auto refhandle = RemoveItemReverse(player_ref, nullptr, realcontainer_formid, RE::ITEM_REMOVE_REASON::kDropping);
        if (refhandle.get()->GetFormID() == native_handle) {
            logger::trace("Native handle is the same as the refhandle refid");
        }
        else logger::trace("Native handle is NOT the same as the refhandle refid");
        // lets make sure that the refid of the real container is still the same and exist in our data
        // if not I wouldnt know which fake form belongs to it?
  //      for (const auto& [chest_ref, cont_ref] : src->data) {
  //          if (cont_ref == refhandle.get()->GetFormID()) {
  //              ok = true;
		//		break;
		//	}
		//}
  //      if (!ok) return RaiseMngrErr("Real container is not registered");
        const auto chest_refid = GetRealContainerChest(native_handle);
        src->data[chest_refid] = refhandle.get()->GetFormID();
        HandleRegistration(refhandle.get().get());
        MsgBoxCallback(1);
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
        listen_container_change = false;
        while (chest->GetWeightInContainer() > weight_limit) {
            auto inventory = chest->GetInventory();
            auto item = inventory.rbegin();
            auto item_obj = item->first;
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

        }
        listen_container_change = true;
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
                auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;
                if (chest_ref == cont_ref) {
                    auto fake_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);
                    is_equipped_x = IsEquipped(fake_bound);
                    is_favorited_x = IsFaved(fake_bound);
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
        logger::info("Data sent.");
    };
    
    void Something1(Source& src, const RefID chest_ref, RE::TESObjectREFR* external_cont = nullptr) {

        logger::trace("Something1");

        const auto saved_ref = src.data[chest_ref];
        // bu sadece load sirasinda
        // ya playerda olcak ya da unownedlardan birinde (containerception)
        // bu ikisi disindaki seylere load_game safhasinda bisey yapamiyorum
        if (!external_cont && chest_ref != saved_ref && !IsChest(saved_ref)) return;

        // saved_ref should not be realcontainer out in the world!
        if (IsRealContainer(external_cont)) {
            logger::critical("saved_ref should not be realcontainer out in the world!");
            return;
        }

        const RefID cont_ref = chest_ref == saved_ref ? 0x14 : saved_ref; // only changing in the case of indication of player has fake container
        const auto cont_of_fakecont = external_cont ? external_cont : RE::TESForm::LookupByID<RE::TESObjectREFR>(cont_ref);
        if (!cont_of_fakecont) return RaiseMngrErr("cont_of_fakecont not found");

        auto fake_formid = ChestToFakeContainer[chest_ref].innerKey;  // dont use this again bcs it can change
        auto fake_cont = RE::TESForm::LookupByID<RE::TESBoundObject>(fake_formid);


        if (fake_cont && fake_cont->IsDeleted()) {
            logger::warn("Fake container with formid {} is deleted. Removing it from inventory/external_container...",
                         fake_formid);
            RemoveItemReverse(cont_of_fakecont, nullptr, fake_formid, RE::ITEM_REMOVE_REASON::kRemove);
        }

        if (!fake_cont || !HasItemPlusCleanUp(fake_cont, cont_of_fakecont)) {
            // here cont_ref becomes unused bcs execute_trick is true
            qTRICK__(src, chest_ref, cont_ref, true);
        }
        else {
            if (!std::strlen(fake_cont->GetName())) {
                logger::warn("Fake container found in {} with empty name.", cont_of_fakecont->GetDisplayFullName());
            }
            // her ihtimale karsi datayi yeniliyorum
            fake_cont->Copy(RE::TESForm::LookupByID<RE::TESForm>(src.formid));
            if (fake_formid != fake_cont->GetFormID()) {
                logger::warn("Fake container formid changed from {} to {}", fake_formid, fake_cont->GetFormID());
            }
            logger::trace("Fake container found in {} with name {} and formid {}.",
                         cont_of_fakecont->GetDisplayFullName(), fake_cont->GetName(), fake_cont->GetFormID());
            qTRICK__(src, chest_ref, cont_ref);
        }
        
        if (chest_ref != saved_ref) {
            // yani unownedlardan birinde olmasi gerekiyo.
            RemoveItemReverse(player_ref, cont_of_fakecont, ChestToFakeContainer[chest_ref].innerKey,
                              RE::ITEM_REMOVE_REASON::kStoreInContainer);
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
        if (!src) return RaiseMngrErr("Could not find source for container");
        Something1(*src, chest_ref);
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
                    if (!fakecontForm.name.empty()) renames[fakecontForm.id] = fakecontForm.name;
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
            RemoveAllItemsFromChest(chest, player_ref);
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
        logger::trace("Equipping and favoriting fake containers in player's inventory");
        auto inventory_changes = player_ref->GetInventoryChanges();
        auto entries = inventory_changes->entryList;
        for (auto it = entries->begin(); it != entries->end(); ++it){
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

#undef ENABLE_IF_NOT_UNINSTALLED

};
