#pragma once

//#include <chrono>
#include <windows.h>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include "SimpleIni.h"
#include <iostream>
#include <string>
#include <codecvt>
#include <mutex>  // for std::mutex
#include <algorithm>
#include <ClibUtil/editorID.hpp>
#include <yaml-cpp/yaml.h>


namespace Utilities {

    // stuff
    const std::string mod_name = static_cast<std::string>(SKSE::PluginDeclaration::GetSingleton()->GetName());
    constexpr auto po3path = "Data/SKSE/Plugins/po3_Tweaks.dll";

    const auto no_src_msgbox = std::format(
        "{}: You currently do not have any container set up. Check your ini file or see the mod page for instructions.",
        mod_name);

    const auto po3_err_msgbox = std::format(
        "{}: You have given an invalid FormID. If you are using Editor IDs, you must have powerofthree's Tweaks "
        "installed. See mod page for further instructions.",
        mod_name);
    const auto general_err_msgbox = std::format("{}: Something went wrong. Please contact the mod author.", mod_name);
    const auto init_err_msgbox = std::format("{}: The mod failed to initialize and will be terminated.", mod_name);

    // string stuff

    std::string DecodeTypeCode(std::uint32_t typeCode);

    bool isValidHexWithLength7or8(const char* input);

    inline bool IsPo3Installed() { return std::filesystem::exists(po3path); };
    
    template <typename Key, typename Value>
    inline void printMap(const std::map<Key, Value>& myMap) {
        for (const auto& pair : myMap) {
			logger::trace("Key: {}, Value: {}", pair.first, pair.second);
		}
	}

    std::string GetPluginVersion(const unsigned int n_stellen);

    
    // message boxes and notifications

    namespace MsgBoxesNotifs {

        // https://github.com/SkyrimScripting/MessageBox/blob/ac0ea32af02766582209e784689eb0dd7d731d57/include/SkyrimScripting/MessageBox.h#L9
        class SkyrimMessageBox {
            class MessageBoxResultCallback : public RE::IMessageBoxCallback {
                std::function<void(unsigned int)> _callback;

            public:
                ~MessageBoxResultCallback() override {}
                MessageBoxResultCallback(std::function<void(unsigned int)> callback) : _callback(callback) {}
                void Run(RE::IMessageBoxCallback::Message message) override {
                    _callback(static_cast<unsigned int>(message));
                }
            };

        public:
            static void Show(const std::string& bodyText, std::vector<std::string> buttonTextValues,
                             std::function<void(unsigned int)> callback);
        };

        inline void ShowMessageBox(const std::string& bodyText, std::vector<std::string> buttonTextValues,
                            std::function<void(unsigned int)> callback) {
            SkyrimMessageBox::Show(bodyText, buttonTextValues, callback);
        }

        namespace Windows {
            inline void Po3ErrMsg();
        };

        namespace InGame {

            inline void InitErr() { RE::DebugMessageBox(init_err_msgbox.c_str()); };

            inline void GeneralErr() { RE::DebugMessageBox(general_err_msgbox.c_str()); };

            inline void FormTypeErr(RE::FormID id) {
				RE::DebugMessageBox(
					std::format("{}: The form type of the item with FormID ({:x}) is not supported. Please contact the mod author.",
						Utilities::mod_name, id).c_str());
			};
            
            inline void ProblemWithContainer(std::string id) {
                RE::DebugMessageBox(
					std::format("{}: Problem with one of the items with the form id ({}). This is expected if you have changed the list of containers in the INI file between saves. Corresponding items will be returned to your inventory. You can suppress this message by changing the setting in your INI.",
                        								Utilities::mod_name, id)
						.c_str());
            };

            inline void UninstallSuccessful() {
				RE::DebugMessageBox(
					std::format("{}: Uninstall successful. You can now safely remove the mod.",
								Utilities::mod_name)
						.c_str());
			};

            inline void UninstallFailed() {
                RE::DebugMessageBox(
                    std::format("{}: Uninstall failed. Please contact the mod author.", Utilities::mod_name).c_str());
            };

            inline void CustomErrMsg(const std::string& msg) { RE::DebugMessageBox((mod_name + ": " + msg).c_str()); };
        };
    };
    
    // Get ID stuff
    // Utility functions
    namespace Functions {

        template <typename Key, typename Value>
        bool containsValue(const std::map<Key, Value>& myMap, const Value& valueToFind) {
            for (const auto& pair : myMap) {
                if (pair.second == valueToFind) {
                    return true;
                }
            }
            return false;
        }

        bool isValidHexWithLength7or8(const char* input);

        namespace String {

            std::vector<std::pair<int, bool>> encodeString(const std::string& inputString);

            std::string decodeString(const std::vector<std::pair<int, bool>>& encodedValues);

        };

    };

    namespace Math {
        namespace LinAlg {
            namespace R3 {
                // Function to rotate a vector around the z-axis
                void rotateZ(RE::NiPoint3& v, float angle);
            };
        };
    };

    namespace FunctionsSkyrim {
    
        static RE::TESForm* GetFormByID(const RE::FormID& id, const std::string& editor_id = "") {
            auto form = RE::TESForm::LookupByID(id);
            if (form)
                return form;
            else if (!editor_id.empty()) {
                form = RE::TESForm::LookupByEditorID(editor_id);
                if (form) return form;
            }
            return nullptr;
        };

        template <class T = RE::TESForm>
        static T* GetFormByID(const RE::FormID& id, const std::string& editor_id = "") {
            T* form = RE::TESForm::LookupByID<T>(id);
            if (form)
                return form;
            else if (!editor_id.empty()) {
                form = RE::TESForm::LookupByEditorID<T>(editor_id);
                if (form) return form;
            }
            return nullptr;
        };

        const std::string GetEditorID(const FormID a_formid);

        FormID GetFormEditorIDFromString(const std::string& formEditorId);

        inline std::size_t GetExtraDataListLength(const RE::ExtraDataList* dataList);

        template <typename T>
        struct FormTraits {
            static float GetWeight(T* form) {
                // Default implementation, assuming T has a member variable 'weight'
                return form->weight;
            }

            static void SetWeight(T* form, float weight) {
                // Default implementation, set the weight if T has a member variable 'weight'
                form->weight = weight;
            }
    
            static int GetValue(T* form) {
			    // Default implementation, assuming T has a member variable 'value'
			    return form->value;
		    }

            static void SetValue(T* form, int value) {
                form->value = value;
            }
        };

        // Specialization for TESAmmo
        template <>
        struct FormTraits<RE::TESAmmo> {
            static float GetWeight(RE::TESAmmo*) {
                // Handle TESAmmo case where 'weight' is not a member
                // You might return a default value or calculate it based on other factors
                return 0.0f;  // For example, returning 0 as a default value
            }

            static void SetWeight(RE::TESAmmo*, float) {
                // Handle setting the weight for TESAmmo
                // (implementation based on your requirements)
                // For example, if TESAmmo had a SetWeight method, you would call it here
            }

            static int GetValue(RE::TESAmmo* form) {
			    return form->value;
		    }
            static void SetValue(RE::TESAmmo* form, int value) {
			    form->value = value;
		    }
        };

        template <>
        struct FormTraits<RE::AlchemyItem> {
            static float GetWeight(RE::AlchemyItem* form) { 
                return form->weight;
            }

            static void SetWeight(RE::AlchemyItem* form, float weight) { 
                form->weight = weight;
            }

            static int GetValue(RE::AlchemyItem* form) {
        	    return form->GetGoldValue();
            }
            static void SetValue(RE::AlchemyItem* form, int value) { 
                logger::trace("CostOverride: {}", form->data.costOverride);
                form->data.costOverride = value;
            }
        };

        namespace xData {

            void AddTextDisplayData(RE::ExtraDataList* extraDataList, const std::string& displayName);

            namespace Copy {
                inline void CopyEnchantment(RE::ExtraEnchantment* from, RE::ExtraEnchantment* to);

                inline void CopyHealth(RE::ExtraHealth* from, RE::ExtraHealth* to);

                inline void CopyRank(RE::ExtraRank* from, RE::ExtraRank* to);

                inline void CopyTimeLeft(RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to);

                inline void CopyCharge(RE::ExtraCharge* from, RE::ExtraCharge* to);

                inline void CopyScale(RE::ExtraScale* from, RE::ExtraScale* to);

                inline void CopyUniqueID(RE::ExtraUniqueID* from, RE::ExtraUniqueID* to);

                inline void CopyPoison(RE::ExtraPoison* from, RE::ExtraPoison* to);

                inline void CopyObjectHealth(RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to);
                // *
                inline void CopyLight(RE::ExtraLight*, RE::ExtraLight*);

                inline void CopyRadius(RE::ExtraRadius* from, RE::ExtraRadius* to);
                // *
                inline void CopyHorse(RE::ExtraHorse*, RE::ExtraHorse*);

                inline void CopyHotkey(RE::ExtraHotkey* from, RE::ExtraHotkey* to);

                inline void CopyTextDisplayData(RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to);

                inline void CopySoul(RE::ExtraSoul* from, RE::ExtraSoul* to);

                inline void CopyOwnership(RE::ExtraOwnership* from, RE::ExtraOwnership* to);
            };

            [[nodiscard]] const bool UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to);

            [[nodiscard]] const bool UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to);

            const int32_t GetXDataCostOverride(RE::ExtraDataList* xList);

        };

        namespace Inventory {
            
            inline const bool EntryHasXData(RE::InventoryEntryData* entry) {
                if (entry && entry->extraLists && !entry->extraLists->empty()) return true;
                return false;
            }

            inline const int32_t GetEntryCostOverride(RE::InventoryEntryData* entry);

            const unsigned int GetValueInContainer(RE::TESObjectREFR* container);

            inline const bool HasItemEntry(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner,
                                    bool nonzero_entry_check = false);

            const std::int32_t GetItemCount(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

            const std::int32_t GetItemValue(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

            inline const bool HasItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
                if (HasItemEntry(item, inventory_owner, true)) return true;
                return false;
            }

            void FavoriteItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

            [[nodiscard]] const bool IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

            [[nodiscard]] const bool IsEquipped(RE::TESBoundObject* item);

            void EquipItem(RE::TESBoundObject* item, bool unequip = false);

        };


        namespace WorldObject {

            void SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok = true);

            void SetObjectCount(RE::TESObjectREFR* ref, Count count);

            RE::TESObjectREFR* DropObjectIntoTheWorld(RE::TESBoundObject* obj, Count count = 1, bool owned = true);

        };
        

        // SkyrimThiago <3
        // https://github.com/Thiago099/DPF-Dynamic-Persistent-Forms
        namespace DynamicForm {

            template <class T>
            void copyComponent(RE::TESForm* from, RE::TESForm* to) {
                auto fromT = from->As<T>();

                auto toT = to->As<T>();

                if (fromT && toT) {
                    toT->CopyComponent(fromT);
                }
            }
        };
    };

    namespace Types {

        // using EditorID = std::string;
        using NameID = std::string;

        // LHS,aka key
        struct FormRefID {
            FormID outerKey;  // real formid
            RefID innerKey;   // refid of unowned

            FormRefID() : outerKey(0), innerKey(0) {}
            FormRefID(FormID value1, RefID value2) : outerKey(value1), innerKey(value2) {}

            bool operator<(const FormRefID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };

        struct FormIDX {
            FormID id;         // fake formid
            bool equipped;     // is equippedB
            bool favorited;    // is favorited
            std::string name;  //(new) name
            FormIDX() : id(0), equipped(false), favorited(false), name("") {}
            FormIDX(FormID id, bool value1, bool value2, std::string value3)
                : id(id), equipped(value1), favorited(value2), name(value3) {}
        };

        // goes to RHS, aka value
        struct FormRefIDX {
            FormIDX outerKey;  // see above
            RefID innerKey;    // refid of unowned/realoutintheworld/externalcont

            FormRefIDX() : innerKey(0) {}
            FormRefIDX(FormIDX value1, RefID value2) : outerKey(value1), innerKey(value2) {}

            bool operator<(const FormRefIDX& other) const {
                // Here, you can access the boolean values via outerKey
                return outerKey.id < other.outerKey.id ||
                       (outerKey.id == other.outerKey.id && innerKey < other.innerKey);
            }
        };

        struct FormFormID {  // used by ChestToFakeContainer
            FormID outerKey;
            FormID innerKey;
            bool operator<(const FormFormID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };

        using SourceDataKey = RefID;  // Chest Ref ID
        using SourceDataVal = RefID;  // Container Ref ID if it exists otherwise Chest Ref ID
        // using SourceData = BidirectionalMap<SourceDataKey, SourceDataVal>; // Chest-Container Reference ID Pairs
        using SourceData = std::map<SourceDataKey, SourceDataVal>;  // Chest-Container Reference ID Pairs
        using SaveDataLHS = FormRefID;
        using SaveDataRHS = FormRefIDX;

        struct SaveDataRHS2 {
            FormID id;       // fake formid
            bool equipped;   // is equipped
            bool favorited;  // is favorited
            RefID refid;     // refid of unowned/realoutintheworld/externalcont

            SaveDataRHS2() : id(0), equipped(false), favorited(false), refid(0) {}
        };

        struct DFSaveData {
            FormID dyn_formid = 0;
            std::pair<bool, uint32_t> custom_id = {false, 0};
            float acteff_elapsed = -1.f;
        };
        using DFSaveDataLHS = std::pair<FormID, std::string>;
        using DFSaveDataRHS = std::vector<DFSaveData>;

    };

    bool read_string(SKSE::SerializationInterface* a_intfc, std::string& a_str);

    bool write_string(SKSE::SerializationInterface* a_intfc, const std::string& a_str);
    // Saving and Loading
    
    // https :  // github.com/ozooma10/OSLAroused/blob/29ac62f220fadc63c829f6933e04be429d4f96b0/src/PersistedData.cpp
    template <typename T,typename U>
    // BaseData is based off how powerof3's did it in Afterlife
    class BaseData {
    public:
        float GetData(T formId, T missing) {
            Locker locker(m_Lock);
            // if the plugin version is less than 0.7 need to handle differently
            // if (SKSE::PluginInfo::version)
            if (auto idx = m_Data.find(formId) != m_Data.end()) {
                return m_Data[formId];
            }
            return missing;
        }

        void SetData(T formId, U value) {
            Locker locker(m_Lock);
            m_Data[formId] = value;
        }

        virtual const char* GetType() = 0;

        virtual bool Save(SKSE::SerializationInterface*, std::uint32_t,
                          std::uint32_t) {return false;};
        virtual bool Save(SKSE::SerializationInterface*) {return false;};
        virtual bool Load(SKSE::SerializationInterface*, const bool) {return false;};

        void Clear() {
            Locker locker(m_Lock);
            m_Data.clear();
        };

        virtual void DumpToLog() = 0;

    protected:
        std::map<T,U> m_Data;

        using Lock = std::recursive_mutex;
        using Locker = std::lock_guard<Lock>;
        mutable Lock m_Lock;
    };

    class SaveLoadData : public BaseData<Types::SaveDataLHS,Types::SaveDataRHS> {
    public:
        void DumpToLog() override {
            Locker locker(m_Lock);
            for (const auto& [formId, value] : m_Data) {
                logger::info(
                    "Dump Row From {} - RealContainerFormID: {} - UnownedRefID: {} - FakeContainerFormID: {} - "
                    "ContainerRefID: {}",
                    GetType(), formId.outerKey, formId.innerKey, value.outerKey.id, value.innerKey);
            }
            // sakat olabilir
            logger::info("{} Rows Dumped For Type {}", m_Data.size(), GetType());
        }

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override {
            assert(serializationInterface);
            Locker locker(m_Lock);

            const auto numRecords = m_Data.size();
            if (!serializationInterface->WriteRecordData(numRecords)) {
                logger::error("Failed to save {} data records", numRecords);
                return false;
            }

            for (const auto& [formId, value] : m_Data) {
                if (!serializationInterface->WriteRecordData(formId)) {
                    logger::error("Failed to save data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                    return false;
                }

                Types::SaveDataRHS2 saveDataRHS;
                saveDataRHS.id = value.outerKey.id;
                saveDataRHS.equipped = value.outerKey.equipped;
                saveDataRHS.favorited = value.outerKey.favorited;
                saveDataRHS.refid = value.innerKey;

                if (!serializationInterface->WriteRecordData(saveDataRHS)) {
                    logger::error("Failed to save value data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                    return false;
                }

                if (!write_string(serializationInterface, value.outerKey.name)) {
					logger::error("Failed to save name data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
					return false;
				}
            }
            return true;
        }

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                           std::uint32_t version) override {
            if (!serializationInterface->OpenRecord(type, version)) {
                logger::error("Failed to open record for Data Serialization!");
                return false;
            }

            return Save(serializationInterface);
        }

        [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, const bool is_older_version) override {
            assert(serializationInterface);

            std::size_t recordDataSize;
            serializationInterface->ReadRecordData(recordDataSize);
            logger::trace("Loading data from serialization interface with size: {}", recordDataSize);

            Locker locker(m_Lock);
            m_Data.clear();


            for (auto i = 0; i < recordDataSize; i++) {
                Types::SaveDataLHS formId;
                Types::SaveDataRHS value;
                logger::trace("Loading data from serialization interface.");
                logger::trace("FormID: ({},{}) serializationInterface->ReadRecordData:{}", formId.outerKey, formId.innerKey,
                            serializationInterface->ReadRecordData(formId));

                if (!serializationInterface->ResolveFormID(formId.outerKey, formId.outerKey)) {
                    logger::error("Failed to resolve form ID, 0x{:X}.", formId.outerKey);
                    continue;
                }
                
                if (is_older_version && !serializationInterface->ReadRecordData(value)) {
                        logger::error("Failed to load value data for FormRefID: ({},{})", formId.outerKey,
                                      formId.innerKey);
                        return false;
                } 
                else {
                    Types::SaveDataRHS2 saveDataRHS;
                    logger::trace("Reading value...");
                    if (!serializationInterface->ReadRecordData(saveDataRHS)) {
					    logger::error("Failed to load value data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
					    return false;
				    }

                    value.outerKey.id = saveDataRHS.id;
                    value.outerKey.equipped = saveDataRHS.equipped;
                    value.outerKey.favorited = saveDataRHS.favorited;
                    value.innerKey = saveDataRHS.refid;

                    if (!read_string(serializationInterface, value.outerKey.name)) {
                    	    logger::error("Failed to load name data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                    }
                }

                m_Data[formId] = value;
                logger::trace("Loaded data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
            }
            return true;
        }
    };

    class DFSaveLoadData : public BaseData<Types::DFSaveDataLHS, Types::DFSaveDataRHS> {
    public:
        void DumpToLog() override {
            // nothing for now
        }

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override {
            assert(serializationInterface);
            Locker locker(m_Lock);

            const auto numRecords = m_Data.size();
            if (!serializationInterface->WriteRecordData(numRecords)) {
                logger::error("Failed to save {} data records", numRecords);
                return false;
            }

            for (const auto& [lhs, rhs] : m_Data) {
                // we serialize formid, editorid, and refid separately
                std::uint32_t formid = lhs.first;
                logger::trace("Formid:{}", formid);
                if (!serializationInterface->WriteRecordData(formid)) {
                    logger::error("Failed to save formid");
                    return false;
                }

                const std::string editorid = lhs.second;
                logger::trace("Editorid:{}", editorid);
                write_string(serializationInterface, editorid);

                // save the number of rhs records
                const auto numRhsRecords = rhs.size();
                if (!serializationInterface->WriteRecordData(numRhsRecords)) {
                    logger::error("Failed to save the size {} of rhs records", numRhsRecords);
                    return false;
                }

                for (const auto& rhs_ : rhs) {
                    logger::trace("size of rhs_: {}", sizeof(rhs_));
                    if (!serializationInterface->WriteRecordData(rhs_)) {
                        logger::error("Failed to save data");
                        return false;
                    }
                }
            }
            return true;
        }

        [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                                std::uint32_t version) override {
            if (!serializationInterface->OpenRecord(type, version)) {
                logger::error("Failed to open record for Data Serialization!");
                return false;
            }

            return Save(serializationInterface);
        }

        [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, const bool) override {
            assert(serializationInterface);

            std::size_t recordDataSize;
            serializationInterface->ReadRecordData(recordDataSize);
            logger::info("Loading data from serialization interface with size: {}", recordDataSize);

            Locker locker(m_Lock);
            m_Data.clear();

            logger::trace("Loading data from serialization interface.");
            for (auto i = 0; i < recordDataSize; i++) {
                Types::DFSaveDataRHS rhs;

                std::uint32_t formid = 0;
                logger::trace("ReadRecordData:{}", serializationInterface->ReadRecordData(formid));
                if (!serializationInterface->ResolveFormID(formid, formid)) {
                    logger::error("Failed to resolve form ID, 0x{:X}.", formid);
                    continue;
                }

                std::string editorid;
                if (!read_string(serializationInterface, editorid)) {
                    logger::error("Failed to read editorid");
                    return false;
                }

                logger::trace("Formid:{}", formid);
                logger::trace("Editorid:{}", editorid);

                Types::DFSaveDataLHS lhs({formid, editorid});
                logger::trace("Reading value...");

                std::size_t rhsSize = 0;
                logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhsSize));
                logger::trace("rhsSize: {}", rhsSize);

                for (auto j = 0; j < rhsSize; j++) {
                    Types::DFSaveData rhs_;
                    logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhs_));
                    logger::trace(
                        "rhs_ content: dyn_formid: {}, customid_bool: {},"
                        "customid: {}, acteff_elapsed: {}",
                        rhs_.dyn_formid, rhs_.custom_id.first, rhs_.custom_id.second, rhs_.acteff_elapsed);
                    rhs.push_back(rhs_);
                }

                m_Data[lhs] = rhs;
                logger::info("Loaded data for formid {}, editorid {}", formid, editorid);
            }

            return true;
        }
    };

};