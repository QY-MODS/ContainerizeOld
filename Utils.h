#pragma once

//#include <chrono>
#include "logger.h"
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




namespace Utilities {

    // stuff
    const auto mod_name = static_cast<std::string>(SKSE::PluginDeclaration::GetSingleton()->GetName());
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
    template <typename T>
    std::string join(const T& container, const std::string_view& delimiter) {
        std::ostringstream oss;
        auto iter = container.begin();

        if (iter != container.end()) {
            oss << *iter;
            ++iter;
        }

        for (; iter != container.end(); ++iter) {
            oss << delimiter << *iter;
        }

        return oss.str();
    }

    bool EqStr(const char* str1, const char* str2) { return std::strcmp(str1, str2) == 0; }

    std::string DecodeTypeCode(std::uint32_t typeCode) {
        char buf[4];
        buf[3] = char(typeCode);
        buf[2] = char(typeCode >> 8);
        buf[1] = char(typeCode >> 16);
        buf[0] = char(typeCode >> 24);
        return std::string(buf, buf + 4);
    }

    std::string generateUniqueName(const std::string& baseName, std::unordered_set<std::string>& nameSet) {
        // Generate a unique name by appending a number
        std::string uniqueName = baseName;
        int counter = 1;

        while (nameSet.find(uniqueName) != nameSet.end()) {
            // If the name is already in the set, append a number and try again
            std::ostringstream oss;
            oss << baseName << counter++;
            uniqueName = oss.str();
        }

        return uniqueName;
    }

    std::string stripPostfix(const std::string& original, const std::string& postfix) {
        if (original.size() >= postfix.size() &&
            original.compare(original.size() - postfix.size(), postfix.size(), postfix) == 0) {
            return original.substr(0, original.size() - postfix.size());
        }
        return original;  // No matching postfix found
    }

    // number stuff
    std::string dec2hex(int dec) {
        std::stringstream stream;
        stream << std::hex << dec;
        std::string hexString = stream.str();
        return hexString;
    };

    bool isValidHexWithLength7or8(const char* input) {
        std::string inputStr(input);
        std::regex hexRegex("^[0-9A-Fa-f]{7,8}$");  // Allow 7 to 8 characters
        bool isValid = std::regex_match(inputStr, hexRegex);
        return isValid;
    }

    float Round(float number, int decimalPlaces) {
        auto rounded_number =
            static_cast<float>(std::round(number * std::pow(10, decimalPlaces))) / std::pow(10, decimalPlaces);
        return rounded_number;
    }

    int Round2Int(float number) {
        auto rounded_number = static_cast<int>(Round(number, 0));
        return rounded_number;
    }

    bool IsPo3Installed() { return std::filesystem::exists(po3path); };
    
    template <typename Key, typename Value>
    void printMap(const std::map<Key, Value>& myMap) {
        for (const auto& pair : myMap) {
			logger::trace("Key: {}, Value: {}", pair.first, pair.second);
		}
	}

    // bidirectional map
    template <typename Key, typename Value>
    class BidirectionalMap {
    public:
        bool insert(const Key& key, const Value& value) {
            // Lock the mutex to ensure thread safety
            //std::lock_guard<std::mutex> lock(mutex);

            auto result1 = keyToValue.insert({key, value});
            auto result2 = valueToKey.insert({value, key});

            // Check if both inserts were successful
            return result1.second && result2.second;
        }

        // Method to insert all entries from another bidirectional map
        bool insertAll(const BidirectionalMap& other) {
            bool success = true;
            for (const auto& [key, value] : other) {
                // Insert the entry into the current bidirectional map
                success = insert(key, value);
                if (!success) {
					// If insertion failed, break out of the loop
					break;
				}
            }
            return success;
        }

        const Value& getValue(const Key& key) const {
            auto it = keyToValue.find(key);
            if (it != keyToValue.end()) {
                return it->second;
            } else {
                logger::info("Key not found in getValue");
                throw std::out_of_range("Key not found in getValue");
            }
        }

        const Key& getKey(const Value& value) const {
            auto it = valueToKey.find(value);
            if (it != valueToKey.end()) {
                return it->second;
            } else {
                logger::info("Value not found in getKey");
                throw std::out_of_range("Value not found in getKey");
            }
        }

        bool containsKey(const Key& key) const { return keyToValue.find(key) != keyToValue.end(); }

        bool containsValue(const Value& value) const { return valueToKey.find(value) != valueToKey.end(); }

        // check at the same time if it exists among both keys or values
        template <typename T>
        bool contains(const T& keyOrValue) const {
            static_assert(std::is_same<Key, T>::value, "Key and Value types must be the same");
            static_assert(std::is_same<Value, T>::value, "Key and Value types must be the same");
            auto keyIt = keyToValue.find(keyOrValue);
            auto valueIt = valueToKey.find(keyOrValue);
            return keyIt != keyToValue.end() || valueIt != valueToKey.end();
        }


        bool updateValue(const Key& key, const Value& newValue) {
            // Lock the mutex to ensure thread safety
            //std::lock_guard<std::mutex> lock(mutex);

            auto keyIt = keyToValue.find(key);
            if (keyIt == keyToValue.end()) {
                logger::warn("updateValue: Key {} not found in keyToValue.", key);
                logger::warn("printing keyToValue");
                printKeyToValue();
                return false;  // Key not found
            }

            const Value& oldValue = keyIt->second;

            if (oldValue == newValue) {
                logger::info("updateValue: New value is same as old value. No update needed.");
                return true;  // Return true to indicate that no update was necessary
            }

            auto valueIt = valueToKey.find(oldValue);
            if (valueIt == valueToKey.end()) {
                logger::warn("updateValue: oldValue {} not found in valueToKey.", oldValue);
                logger::warn("printing valueToKey");
                printValueToKey();
                return false;  // Value not found
            }

            // Update the maps
            logger::trace("updateValue: Key {}, oldValue {}, newValue {}", key, oldValue, newValue);
            logger::trace("keyToValue[{}] = {}", key, keyToValue[key]);
            logger::trace("valueToKey[{}] = {}", oldValue, valueToKey[oldValue]);
            keyIt->second = newValue;
            valueToKey.erase(valueIt);
            valueToKey[newValue] = key;

            return true;
        }

        // Method to remove an entry by key
        bool removeByKey(const Key& key) {
            //std::lock_guard<std::mutex> lock(mutex);

            auto keyIt = keyToValue.find(key);

            if (keyIt != keyToValue.end()) {
                auto valueIt = valueToKey.find(keyIt->second);
                if (valueIt != valueToKey.end()) {
                    keyToValue.erase(keyIt);
                    valueToKey.erase(valueIt);
                    return true;
                }
            }
            return false;  // Key not found
        }

        // Method to remove an entry by value
        bool removeByValue(const Value& value) {
            
            //std::lock_guard<std::mutex> lock(mutex);

            auto valueIt = valueToKey.find(value);
            if (valueIt != valueToKey.end()) {
                auto keyIt = keyToValue.find(valueIt->second);
                if (keyIt != keyToValue.end()) {
                    valueToKey.erase(valueIt);
                    keyToValue.erase(keyIt);
                    return true;
                }
            }
            return false;  // Value not found
        }

        void clear() {

            //std::lock_guard<std::mutex> lock(mutex);

            keyToValue.clear();
            valueToKey.clear();
        }

        auto begin() const {
            //std::lock_guard<std::mutex> lock(mutex);
            return keyToValue.begin();
        }

        auto end() const {
            //std::lock_guard<std::mutex> lock(mutex);
            return keyToValue.end();
        }

        bool isEmpty() const {
            //std::lock_guard<std::mutex> lock(mutex);
            return keyToValue.empty() && valueToKey.empty();
        }

        void printValueToKey() {
            for (const auto& pair : valueToKey) {
                logger::trace("Key: {}, Value: {}", pair.first, pair.second);
            }
        }

        void printKeyToValue() {
            for (const auto& pair : keyToValue) {
				logger::trace("Key: {}, Value: {}", pair.first, pair.second);
			}
		}

    private:
        std::map<Key, Value> keyToValue;
        std::map<Value, Key> valueToKey;
    };

    
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
                             std::function<void(unsigned int)> callback) {
                auto* factoryManager = RE::MessageDataFactoryManager::GetSingleton();
                auto* uiStringHolder = RE::InterfaceStrings::GetSingleton();
                auto* factory = factoryManager->GetCreator<RE::MessageBoxData>(
                    uiStringHolder->messageBoxData);  // "MessageBoxData" <--- can we just use this string?
                auto* messagebox = factory->Create();
                RE::BSTSmartPointer<RE::IMessageBoxCallback> messageCallback =
                    RE::make_smart<MessageBoxResultCallback>(callback);
                messagebox->callback = messageCallback;
                messagebox->bodyText = bodyText;
                for (auto& text : buttonTextValues) messagebox->buttonText.push_back(text.c_str());
                messagebox->QueueMessage();
            }
        };

        void ShowMessageBox(const std::string& bodyText, std::vector<std::string> buttonTextValues,
                            std::function<void(unsigned int)> callback) {
            SkyrimMessageBox::Show(bodyText, buttonTextValues, callback);
        }

        namespace Windows {

            int Po3ErrMsg() {
                MessageBoxA(nullptr, po3_err_msgbox.c_str(), "Error", MB_OK | MB_ICONERROR);
                return 1;
            };

            int GeneralErr() {
                MessageBoxA(nullptr, general_err_msgbox.c_str(), "Error", MB_OK | MB_ICONERROR);
                return 1;
            };
        };

        namespace InGame {

            void IniCreated() { 
                RE::DebugMessageBox("INI created. Customize it to your liking."); 
            };

            void InitErr() { RE::DebugMessageBox(init_err_msgbox.c_str()); };

            void GeneralErr() { RE::DebugMessageBox(general_err_msgbox.c_str()); };

            void NoSourceFound() { RE::DebugMessageBox(no_src_msgbox.c_str()); };

            void FormTypeErr(RE::FormID id) {
				RE::DebugMessageBox(
					std::format("{}: The form type of the item with FormID ({}) is not supported. Please contact the mod author.",
						Utilities::mod_name, Utilities::dec2hex(id)).c_str());
			};
            
            void FormIDError(RE::FormID id) {
                RE::DebugMessageBox(
                    std::format("{}: The ID ({}) could not have been found.",
                                Utilities::mod_name, Utilities::dec2hex(id))
                        .c_str());
            }

            void EditorIDError(std::string id) {
                RE::DebugMessageBox(
                    std::format("{}: The ID ({}) could not have been found.",
                                Utilities::mod_name, id)
                        .c_str());
            }

            void ProblemWithContainer(std::string id) {
                RE::DebugMessageBox(
					std::format("{}: Problem with one of the items with the form id ({}). This is expected if you have changed the list of containers in the INI file between saves. Corresponding items will be returned to your inventory. You can suppress this message by changing the setting in your INI.",
                        								Utilities::mod_name, id)
						.c_str());
            };

            void UninstallSuccessful() {
				RE::DebugMessageBox(
					std::format("{}: Uninstall successful. You can now safely remove the mod.",
								Utilities::mod_name)
						.c_str());
			};

            void UninstallFailed() {
                RE::DebugMessageBox(
                    std::format("{}: Uninstall failed. Please contact the mod author.", Utilities::mod_name).c_str());
            };

            void CustomErrMsg(const std::string& msg) { RE::DebugMessageBox((mod_name + ": " + msg).c_str()); };
        };
    };

    // type stuff
    namespace Types {

        //using EditorID = std::string;
        using NameID = std::string;
        using FormID = RE::FormID;
        using RefID = std::uint32_t;

        struct FormIDX {
            FormID id;
            bool equipped;
            bool favorited;
            std::string name;
            FormIDX() : id(0), equipped(false), favorited(false), name("") {}
            FormIDX(FormID id, bool value1, bool value2, std::string value3)
                : id(id), equipped(value1), favorited(value2), name(value3) {}
        };

        struct FormRefID {
            FormID outerKey;
            RefID innerKey;

            bool operator<(const FormRefID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };

        struct FormRefIDX {
            FormIDX outerKey;
            RefID innerKey;

            bool operator<(const FormRefIDX& other) const {
                // Here, you can access the boolean values via outerKey
                return outerKey.id < other.outerKey.id ||
                       (outerKey.id == other.outerKey.id && innerKey < other.innerKey);
            }
        };

        struct FormFormID {
            FormID outerKey;
            FormID innerKey;

            bool operator<(const FormFormID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };
        
        using SourceDataKey = RefID; // Chest Ref ID
        using SourceDataVal = RefID; // Container Ref ID if it exists otherwise Chest Ref ID
        //using SourceData = BidirectionalMap<SourceDataKey, SourceDataVal>; // Chest-Container Reference ID Pairs
        using SourceData = std::map<SourceDataKey, SourceDataVal>; // Chest-Container Reference ID Pairs
    
    }
    
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

    };

    namespace FunctionsSkyrim {
    
        template <class T = RE::TESForm>
        static T* GetFormByID(const RE::FormID& id, const std::string& editor_id) {
            T* form = RE::TESForm::LookupByID<T>(id);
            if (form)
                return form;
            else if (!editor_id.empty()) {
                form = RE::TESForm::LookupByEditorID<T>(editor_id);
                if (form) return form;
            }
            return nullptr;
        };

        std::size_t GetExtraDataListLength(const RE::ExtraDataList* dataList) {
            std::size_t length = 0;

            for (auto it = dataList->begin(); it != dataList->end(); ++it) {
                // Increment the length for each element in the list
                ++length;
            }

            return length;
        }
        template <class T>
        uint32_t GetLength(T list) {
            logger::trace("Getting length of list");
            uint32_t length = 0;
            for (auto _ : list) {
                ++length;
                logger::trace("Length: {}", length);
            }
            return length;
        }

        template <class T>
        unsigned int GetListLength(RE::BSSimpleList<T>* list) {
            unsigned int count = 0;
            for (const auto& _ : *list) {
                ++count;
            }
            return count;
        }


        bool FormIsOfType(RE::TESForm* form, RE::FormType type) {
		    return form->GetFormType() == type;
	    }

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

        /*float GetBoundObjectWeight(RE::TESBoundObject* object) {
            if (!object) {
                Utilities::MsgBoxesNotifs::ShowMessageBox("Object is null", {"OK"}, [](unsigned int) {});
                return 0;
            }
            std::string formtype(RE::FormTypeToString(object->GetFormType()));
            if (formtype == "ARMO") return FormTraits<RE::TESObjectARMO>::GetWeight(object->As<RE::TESObjectARMO>());
            if (formtype == "WEAP") return FormTraits<RE::TESObjectWEAP>::GetWeight(object->As<RE::TESObjectWEAP>());
            if (formtype == "MISC") return FormTraits<RE::TESObjectMISC>::GetWeight(object->As<RE::TESObjectMISC>());
            if (formtype == "BOOK") return FormTraits<RE::TESObjectBOOK>::GetWeight(object->As<RE::TESObjectBOOK>());
            if (formtype == "AMMO") return FormTraits<RE::TESAmmo>::GetWeight(object->As<RE::TESAmmo>());
            if (formtype == "KEYM") return FormTraits<RE::TESKey>::GetWeight(object->As<RE::TESKey>());
            if (formtype == "SLGM") return FormTraits<RE::TESSoulGem>::GetWeight(object->As<RE::TESSoulGem>());
            if (formtype == "SCRL") return FormTraits<RE::ScrollItem>::GetWeight(object->As<RE::ScrollItem>());
            if (formtype == "LIGH") return FormTraits<RE::TESObjectLIGH>::GetWeight(object->As<RE::TESObjectLIGH>());
            if (formtype == "ALCH") return FormTraits<RE::AlchemyItem>::GetWeight(object->As<RE::AlchemyItem>());

		    return 0;
	    }*/

	    // https:// github.com/Exit-9B/Dont-Eat-Spell-Tomes/blob/7b7f97353cc6e7ccfad813661f39710b46d82972/src/SpellTomeManager.cpp#L23-L32
        template <typename T>
        RE::TESObjectREFR* GetMenuOwner() {
            RE::TESObjectREFR* reference = nullptr;
            const auto ui = RE::UI::GetSingleton();
            const auto menu = ui ? ui->GetMenu<T>() : nullptr;
            const auto movie = menu ? menu->uiMovie : nullptr;

            if (movie) {
                // Parapets: "Menu_mc" is stored in the class, but it's different in VR and we haven't RE'd it yet
                RE::GFxValue isViewingContainer;
                movie->Invoke("Menu_mc.isViewingContainer", &isViewingContainer, nullptr, 0);

                if (isViewingContainer.GetBool()) {
                    auto refHandle = menu->GetTargetRefHandle();
                    RE::TESObjectREFRPtr refr;
                    RE::LookupReferenceByHandle(refHandle, refr);
                    return refr.get();
                }
            }
            return reference;
	    }

        // credits to Qudix on xSE RE Discord for this
        void OpenContainer(RE::TESObjectREFR* a_this, std::uint32_t a_openType) {
            // a_openType is probably in alignment with RE::ContainerMenu::ContainerMode enum
            using func_t = decltype(&OpenContainer);
            REL::Relocation<func_t> func{RELOCATION_ID(50211, 51140)};
            func(a_this, a_openType);
        }

        const unsigned int GetValueInContainer(RE::TESObjectCONT* container) {
			if (!container) {
				logger::warn("Container is null");
				return 0;
			}
            unsigned int total_value = 0;
            for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
                auto entry = container->containerObjects[i];
                if (entry) total_value += entry->obj->GetGoldValue();
                else logger::warn("Entry is null");
            }
            return total_value;
		}

        const unsigned int GetValueInContainer(RE::TESObjectREFR* container) {
            if (!container) {
                logger::warn("Container is null");
                return 0;
            }
            unsigned int total_value = 0;
            auto inventory = container->GetInventory();
            for (auto it = inventory.begin(); it != inventory.end(); ++it) {
                total_value += it->second.second->GetValue() * it->second.first;
			}
            return total_value;
        }

        const bool HasItemEntry(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner, bool nonzero_entry_check=false) {
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

        const std::int32_t GetItemCount(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
            if (HasItemEntry(item, inventory_owner, true)) {
				auto inventory = inventory_owner->GetInventory();
				auto it = inventory.find(item);
				return it->second.first;
			}
            return 0;
        }

        const std::int32_t GetItemValue(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
            if (HasItemEntry(item, inventory_owner, true)) {
                return inventory_owner->GetInventory().find(item)->second.second->GetValue();
            }
            return 0;
        }

        const bool HasItem(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {
            if (HasItemEntry(item, item_owner, true)) return true;
            return false;
        }

        void FavoriteItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
            if (!item) return;
            if (!inventory_owner) return;
            auto inventory_changes = inventory_owner->GetInventoryChanges();
            auto entries = inventory_changes->entryList;
            for (auto it = entries->begin(); it != entries->end(); ++it) {
                auto formid = (*it)->object->GetFormID();
                if (!formid) logger::critical("Formid is null");
                if (formid == item->GetFormID()) {
                    logger::trace("Favoriting item: {}", item->GetName());
                    bool no_extra_ = (*it)->extraLists->empty();
                    logger::trace("asdasd");
                    if (no_extra_) {
                        logger::trace("No extraLists");
                        inventory_changes->SetFavorite((*it), nullptr);
                    } else {
                        logger::trace("ExtraLists found");
                        inventory_changes->SetFavorite((*it), (*it)->extraLists->front());
                    }
                    return;
                }
            }
            logger::error("Item not found in inventory");
        }

        
    [[nodiscard]] const bool IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
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

        /*int GetBoundObjectValue(RE::TESBoundObject* object) {
            if (!object) {
                Utilities::MsgBoxesNotifs::ShowMessageBox("Object is null", {"OK"}, [](unsigned int) {});
                return 0;
            }
            std::string formtype(RE::FormTypeToString(object->GetFormType()));
            if (formtype == "ARMO") FormTraits<RE::TESObjectARMO>::GetValue(object->As<RE::TESObjectARMO>());
            if (formtype == "WEAP") FormTraits<RE::TESObjectWEAP>::GetValue(object->As<RE::TESObjectWEAP>());
            if (formtype == "MISC") FormTraits<RE::TESObjectMISC>::GetValue(object->As<RE::TESObjectMISC>());
            if (formtype == "BOOK") FormTraits<RE::TESObjectBOOK>::GetValue(object->As<RE::TESObjectBOOK>());
            if (formtype == "AMMO") FormTraits<RE::TESAmmo>::GetValue(object->As<RE::TESAmmo>());
            if (formtype == "KEYM") FormTraits<RE::TESKey>::GetValue(object->As<RE::TESKey>());
            if (formtype == "SLGM") FormTraits<RE::TESSoulGem>::GetValue(object->As<RE::TESSoulGem>());
            if (formtype == "SCRL") FormTraits<RE::ScrollItem>::GetValue(object->As<RE::ScrollItem>());
            if (formtype == "LIGH") FormTraits<RE::TESObjectLIGH>::GetValue(object->As<RE::TESObjectLIGH>());
            return 0;
        }*/
    };


    /*void SetBoundObjectValue(RE::TESBoundObject* object, int value) {
        if (!object) {
            Utilities::MsgBoxesNotifs::ShowMessageBox("Object is null", {"OK"}, [](unsigned int) {});
            return;
        }
		std::string formtype(RE::FormTypeToString(object->GetFormType()));
		if (formtype == "ARMO") FormTraits<RE::TESObjectARMO>::SetValue(object->As<RE::TESObjectARMO>(), value);
		if (formtype == "WEAP") FormTraits<RE::TESObjectWEAP>::SetValue(object->As<RE::TESObjectWEAP>(), value);
		if (formtype == "MISC") FormTraits<RE::TESObjectMISC>::SetValue(object->As<RE::TESObjectMISC>(), value);
		if (formtype == "BOOK") FormTraits<RE::TESObjectBOOK>::SetValue(object->As<RE::TESObjectBOOK>(), value);
		if (formtype == "AMMO") FormTraits<RE::TESAmmo>::SetValue(object->As<RE::TESAmmo>(), value);
		if (formtype == "KEYM") FormTraits<RE::TESKey>::SetValue(object->As<RE::TESKey>(), value);
		if (formtype == "SLGM") FormTraits<RE::TESSoulGem>::SetValue(object->As<RE::TESSoulGem>(), value);
		if (formtype == "SCRL") FormTraits<RE::ScrollItem>::SetValue(object->As<RE::ScrollItem>(), value);
		if (formtype == "LIGH") FormTraits<RE::TESObjectLIGH>::SetValue(object->As<RE::TESObjectLIGH>(), value);
	}*/

    // Saving and Loading
    
    // https :  // github.com/ozooma10/OSLAroused/blob/29ac62f220fadc63c829f6933e04be429d4f96b0/src/PersistedData.cpp
    template <typename T>
    // BaseData is based off how powerof3's did it in Afterlife
    class BaseData {
    public:
        float GetData(Types::FormRefID formId, T missing) {
            Locker locker(m_Lock);
            if (auto idx = m_Data.find(formId) != m_Data.end()) {
                return m_Data[formId];
            }
            return missing;
        }

        void SetData(Types::FormRefID formId, T value) {
            Locker locker(m_Lock);
            m_Data[formId] = value;
        }

        virtual const char* GetType() = 0;

        virtual bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                          std::uint32_t version);
        virtual bool Save(SKSE::SerializationInterface* serializationInterface);
        virtual bool Load(SKSE::SerializationInterface* serializationInterface);

        void Clear();

        virtual void DumpToLog() = 0;

    protected:
        std::map<Types::FormRefID, T> m_Data;

        using Lock = std::recursive_mutex;
        using Locker = std::lock_guard<Lock>;
        mutable Lock m_Lock;
    };

    class BaseFormRefIDRefID : public BaseData<Types::RefID> {
    public:
        virtual void DumpToLog() override {
            Locker locker(m_Lock);
            for (const auto& [formId, value] : m_Data) {
                logger::info("Dump Row From {} - ContainerFormID: {} - ContainerRefID: {} - ChestRefID: {}", GetType(),
                             formId.outerKey, formId.innerKey, value);
            }
            // sakat olabilir
            logger::info("{} Rows Dumped For Type {}", m_Data.size(), GetType());
        }
    };

    class BaseFormRefIDFormRefID : public BaseData<Types::FormRefID> {
    public:
        virtual void DumpToLog() override {
            Locker locker(m_Lock);
            for (const auto& [formId, value] : m_Data) {
                logger::info("Dump Row From {} - RealContainerFormID: {} - UnownedRefID: {} - FakeContainerFormID: {} - ContainerRefID: {}", GetType(),
                             formId.outerKey, formId.innerKey, value.outerKey, value.innerKey);
            }
            // sakat olabilir
            logger::info("{} Rows Dumped For Type {}", m_Data.size(), GetType());
        }
    };

    class BaseFormRefIDFormRefIDX : public BaseData<Types::FormRefIDX> {
    public:
        virtual void DumpToLog() override {
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
    };

    // BaseData is based off how powerof3's did it in Afterlife
    template <typename T>
    bool BaseData<T>::Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                           std::uint32_t version) {
        if (!serializationInterface->OpenRecord(type, version)) {
            logger::error("Failed to open record for Data Serialization!");
            return false;
        }

        return Save(serializationInterface);
    }

    template <typename T>
    bool BaseData<T>::Save(SKSE::SerializationInterface* serializationInterface) {
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

            if (!serializationInterface->WriteRecordData(value)) {
                logger::error("Failed to save value data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
                return false;
            }
        }
        return true;
    }

    template <typename T>
    bool BaseData<T>::Load(SKSE::SerializationInterface* serializationInterface) {
        assert(serializationInterface);

        std::size_t recordDataSize;
        serializationInterface->ReadRecordData(recordDataSize);
        logger::trace("Loading data from serialization interface with size: {}", recordDataSize);

        Locker locker(m_Lock);
        m_Data.clear();

        Types::FormRefID formId;
        T value;

        for (auto i = 0; i < recordDataSize; i++) {
            logger::trace("Loading data from serialization interface.");
            auto asdasdf = serializationInterface->ReadRecordData(formId);
            logger::trace("FormID: ({},{}) serializationInterface->ReadRecordData:{}", formId.outerKey, formId.innerKey,
                          asdasdf);

            if (!serializationInterface->ResolveFormID(formId.outerKey, formId.outerKey)) {
                logger::error("Failed to resolve form ID, 0x{:X}.", formId.outerKey);
                continue;
            }

            logger::trace("Reading value...");
            auto sdddasd = serializationInterface->ReadRecordData(value);
            logger::trace("ReadRecordData: {}", sdddasd);
            m_Data[formId] = value;
            logger::trace("Loaded data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
        }
        return true;
    }

    template <typename T>
    void BaseData<T>::Clear() {
        Locker locker(m_Lock);
        m_Data.clear();
    }

};