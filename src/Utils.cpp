#include "Utils.h"

std::string Utilities::DecodeTypeCode(std::uint32_t typeCode) {
    char buf[4];
    buf[3] = char(typeCode);
    buf[2] = char(typeCode >> 8);
    buf[1] = char(typeCode >> 16);
    buf[0] = char(typeCode >> 24);
    return std::string(buf, buf + 4);
}

bool Utilities::isValidHexWithLength7or8(const char* input) {
    std::string inputStr(input);
    std::regex hexRegex("^[0-9A-Fa-f]{7,8}$");  // Allow 7 to 8 characters
    bool isValid = std::regex_match(inputStr, hexRegex);
    return isValid;
}

std::string Utilities::GetPluginVersion(const unsigned int n_stellen) {
    const auto fullVersion = SKSE::PluginDeclaration::GetSingleton()->GetVersion();
    unsigned int i = 1;
    std::string version = std::to_string(fullVersion.major());
    if (n_stellen == i) return version;
    version += "." + std::to_string(fullVersion.minor());
    if (n_stellen == ++i) return version;
    version += "." + std::to_string(fullVersion.patch());
    if (n_stellen == ++i) return version;
    version += "." + std::to_string(fullVersion.build());
    return version;
}

bool Utilities::read_string(SKSE::SerializationInterface* a_intfc, std::string& a_str) {
    std::vector<std::pair<int, bool>> encodedStr;
    std::size_t size;
    if (!a_intfc->ReadRecordData(size)) {
        return false;
    }
    for (std::size_t i = 0; i < size; i++) {
        std::pair<int, bool> temp_pair;
        if (!a_intfc->ReadRecordData(temp_pair)) {
            return false;
        }
        encodedStr.push_back(temp_pair);
    }
    a_str = Functions::String::decodeString(encodedStr);
    return true;
}

bool Utilities::write_string(SKSE::SerializationInterface* a_intfc, const std::string& a_str) {
    auto encodedStr = Functions::String::encodeString(a_str);
    // i first need the size to know no of iterations
    const auto size = encodedStr.size();
    if (!a_intfc->WriteRecordData(size)) {
        return false;
    }
    for (const auto& temp_pair : encodedStr) {
        if (!a_intfc->WriteRecordData(temp_pair)) {
            return false;
        }
    }
    return true;
}


void Utilities::MsgBoxesNotifs::SkyrimMessageBox::Show(const std::string& bodyText,
                                                       std::vector<std::string> buttonTextValues,
                                                       std::function<void(unsigned int)> callback) {
    auto* factoryManager = RE::MessageDataFactoryManager::GetSingleton();
    auto* uiStringHolder = RE::InterfaceStrings::GetSingleton();
    auto* factory = factoryManager->GetCreator<RE::MessageBoxData>(
        uiStringHolder->messageBoxData);  // "MessageBoxData" <--- can we just use this string?
    auto* messagebox = factory->Create();
    RE::BSTSmartPointer<RE::IMessageBoxCallback> messageCallback = RE::make_smart<MessageBoxResultCallback>(callback);
    messagebox->callback = messageCallback;
    messagebox->bodyText = bodyText;
    for (auto& text : buttonTextValues) messagebox->buttonText.push_back(text.c_str());
    messagebox->QueueMessage();
}

void Utilities::MsgBoxesNotifs::Windows::Po3ErrMsg() {
    MessageBoxA(nullptr, po3_err_msgbox.c_str(), "Error", MB_OK | MB_ICONERROR);
}

bool Utilities::Functions::isValidHexWithLength7or8(const char* input) {
    std::string inputStr(input);

    if (inputStr.substr(0, 2) == "0x") {
        // Remove "0x" from the beginning of the string
        inputStr = inputStr.substr(2);
    }

    std::regex hexRegex("^[0-9A-Fa-f]{7,8}$");  // Allow 7 to 8 characters
    bool isValid = std::regex_match(inputStr, hexRegex);
    return isValid;
}

std::vector<std::pair<int, bool>> Utilities::Functions::String::encodeString(const std::string& inputString) {
    std::vector<std::pair<int, bool>> encodedValues;
    try {
        for (int i = 0; i < 100 && inputString[i] != '\0'; i++) {
            char ch = inputString[i];
            if (std::isprint(ch) && (std::isalnum(ch) || std::isspace(ch) || std::ispunct(ch)) && ch >= 0 &&
                ch <= 255) {
                encodedValues.push_back(std::make_pair(static_cast<int>(ch), std::isupper(ch)));
            }
        }
    } catch (const std::exception& e) {
        logger::error("Error encoding string: {}", e.what());
        return encodeString("ERROR");
    }
    return encodedValues;
}

std::string Utilities::Functions::String::decodeString(const std::vector<std::pair<int, bool>>& encodedValues) {
    std::string decodedString;
    for (const auto& pair : encodedValues) {
        char ch = static_cast<char>(pair.first);
        if (std::isalnum(ch) || std::isspace(ch) || std::ispunct(ch)) {
            if (pair.second) {
                decodedString += ch;
            } else {
                decodedString += static_cast<char>(std::tolower(ch));
            }
        }
    }
    return decodedString;
}

void Utilities::Math::LinAlg::R3::rotateZ(RE::NiPoint3& v, float angle) {
    float x = v.x * cos(angle) - v.y * sin(angle);
    float y = v.x * sin(angle) + v.y * cos(angle);
    v.x = x;
    v.y = y;
}

const std::string Utilities::FunctionsSkyrim::GetEditorID(const FormID a_formid) {
    if (const auto form = RE::TESForm::LookupByID(a_formid)) {
        return clib_util::editorID::get_editorID(form);
    } else {
        return "";
    }
}

FormID Utilities::FunctionsSkyrim::GetFormEditorIDFromString(const std::string& formEditorId) {
    logger::trace("Getting formid from editorid: {}", formEditorId);
    if (Utilities::Functions::isValidHexWithLength7or8(formEditorId.c_str())) {
        logger::trace("formEditorId is in hex format.");
        int form_id_;
        std::stringstream ss;
        ss << std::hex << formEditorId;
        ss >> form_id_;
        auto temp_form = GetFormByID(form_id_, "");
        if (temp_form)
            return temp_form->GetFormID();
        else {
            logger::error("Formid is null for editorid {}", formEditorId);
            return 0;
        }
    }
    if (formEditorId.empty()) return 0;
    else if (!IsPo3Installed()) {
        logger::error("Po3 is not installed.");
        MsgBoxesNotifs::Windows::Po3ErrMsg();
        return 0;
    }
    auto temp_form = GetFormByID(0, formEditorId);
    if (temp_form) {
        logger::trace("Formid is not null with formid {}", temp_form->GetFormID());
        return temp_form->GetFormID();
    } else {
        logger::trace("Formid is null for editorid {}", formEditorId);
        return 0;
    }
}

std::size_t Utilities::FunctionsSkyrim::GetExtraDataListLength(const RE::ExtraDataList* dataList) {
    std::size_t length = 0;
    for (auto it = dataList->begin(); it != dataList->end(); ++it) {
        // Increment the length for each element in the list
        ++length;
    }
    return length;
}

void Utilities::FunctionsSkyrim::xData::AddTextDisplayData(RE::ExtraDataList* extraDataList,
                                                           const std::string& displayName) {
    if (!extraDataList) return;
    if (extraDataList->HasType(RE::ExtraDataType::kTextDisplayData)) return;
    auto textDisplayData = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
    textDisplayData->SetName(displayName.c_str());
    extraDataList->Add(textDisplayData);
}

const bool Utilities::FunctionsSkyrim::xData::UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to) {
    logger::trace("UpdateExtras");
    if (!copy_from || !copy_to) return false;
    // Enchantment
    if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
        logger::trace("Enchantment found");
        auto enchantment = static_cast<RE::ExtraEnchantment*>(copy_from->GetByType(RE::ExtraDataType::kEnchantment));
        if (enchantment) {
            RE::ExtraEnchantment* enchantment_fake = RE::BSExtraData::Create<RE::ExtraEnchantment>();
            // log the associated actor value
            logger::trace("Associated actor value: {}", enchantment->enchantment->GetAssociatedSkill());
            Copy::CopyEnchantment(enchantment, enchantment_fake);
            copy_to->Add(enchantment_fake);
        } else
            return false;
    }
    // Health
    if (copy_from->HasType(RE::ExtraDataType::kHealth)) {
        logger::trace("Health found");
        auto health = static_cast<RE::ExtraHealth*>(copy_from->GetByType(RE::ExtraDataType::kHealth));
        if (health) {
            RE::ExtraHealth* health_fake = RE::BSExtraData::Create<RE::ExtraHealth>();
            Copy::CopyHealth(health, health_fake);
            copy_to->Add(health_fake);
        } else
            return false;
    }
    // Rank
    if (copy_from->HasType(RE::ExtraDataType::kRank)) {
        logger::trace("Rank found");
        auto rank = static_cast<RE::ExtraRank*>(copy_from->GetByType(RE::ExtraDataType::kRank));
        if (rank) {
            RE::ExtraRank* rank_fake = RE::BSExtraData::Create<RE::ExtraRank>();
            Copy::CopyRank(rank, rank_fake);
            copy_to->Add(rank_fake);
        } else
            return false;
    }
    // TimeLeft
    if (copy_from->HasType(RE::ExtraDataType::kTimeLeft)) {
        logger::trace("TimeLeft found");
        auto timeleft = static_cast<RE::ExtraTimeLeft*>(copy_from->GetByType(RE::ExtraDataType::kTimeLeft));
        if (timeleft) {
            RE::ExtraTimeLeft* timeleft_fake = RE::BSExtraData::Create<RE::ExtraTimeLeft>();
            Copy::CopyTimeLeft(timeleft, timeleft_fake);
            copy_to->Add(timeleft_fake);
        } else
            return false;
    }
    // Charge
    if (copy_from->HasType(RE::ExtraDataType::kCharge)) {
        logger::trace("Charge found");
        auto charge = static_cast<RE::ExtraCharge*>(copy_from->GetByType(RE::ExtraDataType::kCharge));
        if (charge) {
            RE::ExtraCharge* charge_fake = RE::BSExtraData::Create<RE::ExtraCharge>();
            Copy::CopyCharge(charge, charge_fake);
            copy_to->Add(charge_fake);
        } else
            return false;
    }
    // Scale
    if (copy_from->HasType(RE::ExtraDataType::kScale)) {
        logger::trace("Scale found");
        auto scale = static_cast<RE::ExtraScale*>(copy_from->GetByType(RE::ExtraDataType::kScale));
        if (scale) {
            RE::ExtraScale* scale_fake = RE::BSExtraData::Create<RE::ExtraScale>();
            Copy::CopyScale(scale, scale_fake);
            copy_to->Add(scale_fake);
        } else
            return false;
    }
    // UniqueID
    if (copy_from->HasType(RE::ExtraDataType::kUniqueID)) {
        logger::trace("UniqueID found");
        auto uniqueid = static_cast<RE::ExtraUniqueID*>(copy_from->GetByType(RE::ExtraDataType::kUniqueID));
        if (uniqueid) {
            RE::ExtraUniqueID* uniqueid_fake = RE::BSExtraData::Create<RE::ExtraUniqueID>();
            Copy::CopyUniqueID(uniqueid, uniqueid_fake);
            copy_to->Add(uniqueid_fake);
        } else
            return false;
    }
    // Poison
    if (copy_from->HasType(RE::ExtraDataType::kPoison)) {
        logger::trace("Poison found");
        auto poison = static_cast<RE::ExtraPoison*>(copy_from->GetByType(RE::ExtraDataType::kPoison));
        if (poison) {
            RE::ExtraPoison* poison_fake = RE::BSExtraData::Create<RE::ExtraPoison>();
            Copy::CopyPoison(poison, poison_fake);
            copy_to->Add(poison_fake);
        } else
            return false;
    }
    // ObjectHealth
    if (copy_from->HasType(RE::ExtraDataType::kObjectHealth)) {
        logger::trace("ObjectHealth found");
        auto objhealth = static_cast<RE::ExtraObjectHealth*>(copy_from->GetByType(RE::ExtraDataType::kObjectHealth));
        if (objhealth) {
            RE::ExtraObjectHealth* objhealth_fake = RE::BSExtraData::Create<RE::ExtraObjectHealth>();
            Copy::CopyObjectHealth(objhealth, objhealth_fake);
            copy_to->Add(objhealth_fake);
        } else
            return false;
    }
    // Light
    if (copy_from->HasType(RE::ExtraDataType::kLight)) {
        logger::trace("Light found");
        auto light = static_cast<RE::ExtraLight*>(copy_from->GetByType(RE::ExtraDataType::kLight));
        if (light) {
            RE::ExtraLight* light_fake = RE::BSExtraData::Create<RE::ExtraLight>();
            Copy::CopyLight(light, light_fake);
            copy_to->Add(light_fake);
        } else
            return false;
    }
    // Radius
    if (copy_from->HasType(RE::ExtraDataType::kRadius)) {
        logger::trace("Radius found");
        auto radius = static_cast<RE::ExtraRadius*>(copy_from->GetByType(RE::ExtraDataType::kRadius));
        if (radius) {
            RE::ExtraRadius* radius_fake = RE::BSExtraData::Create<RE::ExtraRadius>();
            Copy::CopyRadius(radius, radius_fake);
            copy_to->Add(radius_fake);
        } else
            return false;
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
            Copy::CopyHorse(horse, horse_fake);
            copy_to->Add(horse_fake);
        } else
            return false;
    }
    // Hotkey
    if (copy_from->HasType(RE::ExtraDataType::kHotkey)) {
        logger::trace("Hotkey found");
        auto hotkey = static_cast<RE::ExtraHotkey*>(copy_from->GetByType(RE::ExtraDataType::kHotkey));
        if (hotkey) {
            RE::ExtraHotkey* hotkey_fake = RE::BSExtraData::Create<RE::ExtraHotkey>();
            Copy::CopyHotkey(hotkey, hotkey_fake);
            copy_to->Add(hotkey_fake);
        } else
            return false;
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
            Copy::CopyTextDisplayData(textdisplaydata, textdisplaydata_fake);
            copy_to->Add(textdisplaydata_fake);
        } else
            return false;
    }
    // Soul
    if (copy_from->HasType(RE::ExtraDataType::kSoul)) {
        logger::trace("Soul found");
        auto soul = static_cast<RE::ExtraSoul*>(copy_from->GetByType(RE::ExtraDataType::kSoul));
        if (soul) {
            RE::ExtraSoul* soul_fake = RE::BSExtraData::Create<RE::ExtraSoul>();
            Copy::CopySoul(soul, soul_fake);
            copy_to->Add(soul_fake);
        } else
            return false;
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
            return false;
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
            Copy::CopyOwnership(ownership, ownership_fake);
            copy_to->Add(ownership_fake);
            logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
        } else
            return false;
    }

    return true;
}

const bool Utilities::FunctionsSkyrim::xData::UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to) {
    logger::trace("UpdateExtras");
    if (!copy_from || !copy_to) {
        logger::error("copy_from or copy_to is null");
        return false;
    }
    auto* copy_from_extralist = &copy_from->extraList;
    auto* copy_to_extralist = &copy_to->extraList;
    return UpdateExtras(copy_from_extralist, copy_to_extralist);
}

const int32_t Utilities::FunctionsSkyrim::xData::GetXDataCostOverride(RE::ExtraDataList* xList) {
    if (!xList) return 0;
    int32_t extra_costs = 0;
    if (xList && GetExtraDataListLength(xList)) {
        if (auto xench = xList->GetByType<RE::ExtraEnchantment>()) {
            auto ench = xench->enchantment;
            auto temp_costoverride = ench->data.costOverride;
            if (temp_costoverride < 0) temp_costoverride = static_cast<int32_t>(ench->CalculateTotalGoldValue());
            if (temp_costoverride < 0)
                temp_costoverride =
                    static_cast<int32_t>(ench->CalculateTotalGoldValue(RE::PlayerCharacter::GetSingleton()));
            if (temp_costoverride > 0) {
                logger::trace("CostOverride: {}", temp_costoverride);
                extra_costs += temp_costoverride;
            }
        }
    }
    return extra_costs;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyEnchantment(RE::ExtraEnchantment* from, RE::ExtraEnchantment* to) {
    logger::trace("CopyEnchantment");
    to->enchantment = from->enchantment;
    to->charge = from->charge;
    to->removeOnUnequip = from->removeOnUnequip;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyHealth(RE::ExtraHealth* from, RE::ExtraHealth* to) {
    logger::trace("CopyHealth");
    to->health = from->health;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyRank(RE::ExtraRank* from, RE::ExtraRank* to) {
    logger::trace("CopyRank");
    to->rank = from->rank;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyTimeLeft(RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to) {
    logger::trace("CopyTimeLeft");
    to->time = from->time;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyCharge(RE::ExtraCharge* from, RE::ExtraCharge* to) {
    logger::trace("CopyCharge");
    to->charge = from->charge;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyScale(RE::ExtraScale* from, RE::ExtraScale* to) {
    logger::trace("CopyScale");
    to->scale = from->scale;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyUniqueID(RE::ExtraUniqueID* from, RE::ExtraUniqueID* to) {
    logger::trace("CopyUniqueID");
    to->baseID = from->baseID;
    to->uniqueID = from->uniqueID;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyPoison(RE::ExtraPoison* from, RE::ExtraPoison* to) {
    logger::trace("CopyPoison");
    to->poison = from->poison;
    to->count = from->count;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyObjectHealth(RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to) {
    logger::trace("CopyObjectHealth");
    to->health = from->health;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyLight(RE::ExtraLight*, RE::ExtraLight*) {
    logger::trace("CopyLight");
    return;
    // to->lightData = from->lightData;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyRadius(RE::ExtraRadius* from, RE::ExtraRadius* to) {
    logger::trace("CopyRadius");
    to->radius = from->radius;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyHorse(RE::ExtraHorse*, RE::ExtraHorse*) {
    logger::trace("CopyHorse");
    return;
    // to->horseRef = from->horseRef;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyHotkey(RE::ExtraHotkey* from, RE::ExtraHotkey* to) {
    logger::trace("CopyHotkey");
    to->hotkey = from->hotkey;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyTextDisplayData(RE::ExtraTextDisplayData* from,
                                                                  RE::ExtraTextDisplayData* to) {
    to->displayName = from->displayName;
    to->displayNameText = from->displayNameText;
    to->ownerQuest = from->ownerQuest;
    to->ownerInstance = from->ownerInstance;
    to->temperFactor = from->temperFactor;
    to->customNameLength = from->customNameLength;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopySoul(RE::ExtraSoul* from, RE::ExtraSoul* to) {
    logger::trace("CopySoul");
    to->soul = from->soul;
}

void Utilities::FunctionsSkyrim::xData::Copy::CopyOwnership(RE::ExtraOwnership* from, RE::ExtraOwnership* to) {
    logger::trace("CopyOwnership");
    to->owner = from->owner;
}

const int32_t Utilities::FunctionsSkyrim::Inventory::GetEntryCostOverride(RE::InventoryEntryData* entry) {
    if (!EntryHasXData(entry)) return 0;
    int32_t extra_costs = 0;
    for (auto& xList : *entry->extraLists) {
        extra_costs += xData::GetXDataCostOverride(xList);
    }
    return extra_costs;
}

const unsigned int Utilities::FunctionsSkyrim::Inventory::GetValueInContainer(RE::TESObjectREFR* container) {
    if (!container) {
        logger::warn("Container is null");
        return 0;
    }
    unsigned int total_value = 0;
    auto inventory = container->GetInventory();
    for (auto it = inventory.begin(); it != inventory.end(); ++it) {
        auto gold_value = it->first->GetGoldValue();
        logger::trace("Gold value: {}", gold_value);
        total_value += gold_value * it->second.first;
        int extra_costs = 0;
        /*if (auto ench = it->second.second->GetEnchantment()) {
            auto costoverride = ench->GetData()->costOverride;
            logger::trace("Base enchantment cost: {}", costoverride);
            extra_costs += costoverride;
        }*/
        extra_costs += GetEntryCostOverride(it->second.second.get());
        total_value += extra_costs;
    }
    return total_value;
}

const bool Utilities::FunctionsSkyrim::Inventory::HasItemEntry(RE::TESBoundObject* item,
                                                               RE::TESObjectREFR* inventory_owner,
                                                               bool nonzero_entry_check) {
    /*if (const auto has_entry = inventory.find(item) != inventory.end(); !has_entry) return false;
    else return nonzero_entry_check ? has_entry && inventory.at(item).first > 0 : has_entry;*/

    if (!item) {
        logger::warn("Item is null");
        return 0;
    }
    if (!inventory_owner) {
        logger::warn("Inventory owner is null");
        return 0;
    }
    auto inventory = inventory_owner->GetInventory();
    auto it = inventory.find(item);
    bool has_entry = it != inventory.end();
    if (nonzero_entry_check) return has_entry && it->second.first > 0;
    return has_entry;
}

const std::int32_t Utilities::FunctionsSkyrim::Inventory::GetItemCount(RE::TESBoundObject* item,
                                                                       RE::TESObjectREFR* inventory_owner) {
    /*if (!HasItemEntry(item, inventory, true)) return 0;
    return inventory.find(item)->second.first;*/
    if (HasItemEntry(item, inventory_owner, true)) {
        auto inventory = inventory_owner->GetInventory();
        auto it = inventory.find(item);
        return it->second.first;
    }
    return 0;


}

const std::int32_t Utilities::FunctionsSkyrim::Inventory::GetItemValue(RE::TESBoundObject* item,
                                                                       RE::TESObjectREFR* inventory_owner) {
    /*if (HasItemEntry(item, inventory, true)) {
        return inventory.at(item).second->GetValue();
    }
    return 0;*/
    if (HasItemEntry(item, inventory_owner, true)) {
        return inventory_owner->GetInventory().find(item)->second.second->GetValue();
    }
    return 0;
}

void Utilities::FunctionsSkyrim::Inventory::FavoriteItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
    if (!item) return;
    if (!inventory_owner) return;
    auto inventory_changes = inventory_owner->GetInventoryChanges();
    auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        if (!(*it)) {
            logger::error("Item entry is null");
            continue;
        }
        const auto object = (*it)->object;
        if (!object) {
            logger::error("Object is null");
            continue;
        }
        auto formid = object->GetFormID();
        if (!formid) logger::critical("Formid is null");
        if (formid == item->GetFormID()) {
            logger::trace("Favoriting item: {}", item->GetName());
            const auto xLists = (*it)->extraLists;
            bool no_extra_ = false;
            if (!xLists || xLists->empty()) {
                logger::trace("No extraLists");
                no_extra_ = true;
            }
            logger::trace("asdasd");
            if (no_extra_) {
                logger::trace("No extraLists");
                // inventory_changes->SetFavorite((*it), nullptr);
            } else if (xLists->front()) {
                logger::trace("ExtraLists found");
                inventory_changes->SetFavorite((*it), xLists->front());
            }
            return;
        }
    }
    logger::error("Item not found in inventory");
}

const bool Utilities::FunctionsSkyrim::Inventory::IsFavorited(RE::TESBoundObject* item,
                                                              RE::TESObjectREFR* inventory_owner) {
    if (!item) {
        logger::warn("Item is null");
        return false;
    }
    if (!inventory_owner) {
        logger::warn("Inventory owner is null");
        return false;
    }
    auto inventory = inventory_owner->GetInventory();
    auto it = inventory.find(item);
    if (it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsFavorited();
    }
    return false;
}

const bool Utilities::FunctionsSkyrim::Inventory::IsEquipped(RE::TESBoundObject* item) {
    logger::trace("IsEquipped");

    if (!item) {
        logger::trace("Item is null");
        return false;
    }

    auto player_ref = RE::PlayerCharacter::GetSingleton();
    auto inventory = player_ref->GetInventory();
    auto it = inventory.find(item);
    if (it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsWorn();
    }
    return false;
}

void Utilities::FunctionsSkyrim::Inventory::EquipItem(RE::TESBoundObject* item, bool unequip) {
    logger::trace("EquipItem");

    if (!item) {
        logger::error("Item is null");
        return;
    }
    auto player_ref = RE::PlayerCharacter::GetSingleton();
    auto inventory_changes = player_ref->GetInventoryChanges();
    auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        auto formid = (*it)->object->GetFormID();
        if (formid == item->GetFormID()) {
            if (!(*it) || !(*it)->extraLists) {
                logger::error("Item extraLists is null");
                return;
            }
            if (unequip) {
                if ((*it)->extraLists->empty()) {
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        player_ref, (*it)->object, nullptr, 1, (const RE::BGSEquipSlot*)nullptr, true, false, false);
                } else if ((*it)->extraLists->front()) {
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        player_ref, (*it)->object, (*it)->extraLists->front(), 1, (const RE::BGSEquipSlot*)nullptr,
                        true, false, false);
                }
            } else {
                if ((*it)->extraLists->empty()) {
                    RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref, (*it)->object, nullptr, 1,
                                                                       (const RE::BGSEquipSlot*)nullptr, true, false,
                                                                       false, false);
                } else if ((*it)->extraLists->front()) {
                    RE::ActorEquipManager::GetSingleton()->EquipObject(
                        player_ref, (*it)->object, (*it)->extraLists->front(), 1, (const RE::BGSEquipSlot*)nullptr,
                        true, false, false, false);
                }
            }
            return;
        }
    }
}

void Utilities::FunctionsSkyrim::WorldObject::SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to,
                                                          const bool apply_havok) {
    logger::trace("SwapObjects");
    if (!a_from) {
        logger::error("Ref is null.");
        return;
    }
    auto ref_base = a_from->GetBaseObject();
    if (!ref_base) {
        logger::error("Ref base is null.");
        return;
    }
    if (!a_to) {
        logger::error("Base is null.");
        return;
    }
    if (ref_base->GetFormID() == a_to->GetFormID()) {
        logger::trace("Ref and base are the same.");
        return;
    }
    a_from->SetObjectReference(a_to);
    if (!apply_havok) return;
    SKSE::GetTaskInterface()->AddTask([a_from]() {
        a_from->Disable();
        a_from->Enable(false);
    });
    /*a_from->Disable();
    a_from->Enable(false);*/

    /*float afX = 100;
    float afY = 100;
    float afZ = 100;
    float afMagnitude = 100;*/
    /*auto args = RE::MakeFunctionArguments(std::move(afX), std::move(afY), std::move(afZ),
    std::move(afMagnitude)); vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback);*/
    // Looked up here (wSkeever): https:  // www.nexusmods.com/skyrimspecialedition/mods/73607
    /*SKSE::GetTaskInterface()->AddTask([a_from]() {
        ApplyHavokImpulse(a_from, 0.f, 0.f, 10.f, 5000.f);
    });*/
    // SKSE::GetTaskInterface()->AddTask([a_from]() {
    //     // auto player_ch = RE::PlayerCharacter::GetSingleton();
    //     // player_ch->StartGrabObject();
    //     auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    //     auto policy = vm->GetObjectHandlePolicy();
    //     auto handle = policy->GetHandleForObject(a_from->GetFormType(), a_from);
    //     RE::BSTSmartPointer<RE::BSScript::Object> object;
    //     vm->CreateObject2("ObjectReference", object);
    //     if (!object) logger::warn("Object is null");
    //     vm->BindObject(object, handle, false);
    //     auto args = RE::MakeFunctionArguments(std::move(0.f), std::move(0.f), std::move(1.f),
    //     std::move(5.f)); RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback; if
    //     (vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback)) logger::trace("FUSRODAH");
    // });
}

void Utilities::FunctionsSkyrim::WorldObject::SetObjectCount(RE::TESObjectREFR* ref, Count count) {
    if (!ref) {
        logger::error("Ref is null.");
        return;
    }
    int max_try = 10;
    while (ref->extraList.HasType(RE::ExtraDataType::kCount) && max_try) {
        ref->extraList.RemoveByType(RE::ExtraDataType::kCount);
        max_try--;
    }
    // ref->extraList.SetCount(static_cast<uint16_t>(count));
    auto xCount = new RE::ExtraCount(static_cast<int16_t>(count));
    ref->extraList.Add(xCount);
}

RE::TESObjectREFR* Utilities::FunctionsSkyrim::WorldObject::DropObjectIntoTheWorld(RE::TESBoundObject* obj, Count count,
                                                                                   bool owned) {
    auto player_ch = RE::PlayerCharacter::GetSingleton();
    if (!player_ch) {
        logger::critical("Player character is null.");
        return nullptr;
    }
    // PRINT IT
    const auto multiplier = 100.0f;
    const float qPI = 3.14159265358979323846f;
    auto orji_vec = RE::NiPoint3{multiplier, 0.f, player_ch->GetHeight()};
    Math::LinAlg::R3::rotateZ(orji_vec, qPI / 4.f - player_ch->GetAngleZ());
    auto drop_pos = player_ch->GetPosition() + orji_vec;
    auto player_cell = player_ch->GetParentCell();
    auto player_ws = player_ch->GetWorldspace();
    if (!player_cell && !player_ws) {
        logger::critical("Player cell AND player world is null.");
        return nullptr;
    }
    auto newPropRef = RE::TESDataHandler::GetSingleton()
                          ->CreateReferenceAtLocation(obj, drop_pos, {0.0f, 0.0f, 0.0f}, player_cell, player_ws,
                                                      nullptr, nullptr, {}, false, false)
                          .get()
                          .get();
    if (!newPropRef) {
        logger::critical("New prop ref is null.");
        return nullptr;
    }
    if (count > 1) SetObjectCount(newPropRef, count);
    if (owned) newPropRef->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
    return newPropRef;
}
