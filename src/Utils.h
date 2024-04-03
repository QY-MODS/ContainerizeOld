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
        float rounded_number = static_cast<float>(std::round(number * std::pow(10, decimalPlaces))) /static_cast<float>(std::pow(10, decimalPlaces));
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

    std::string GetPluginVersion(const unsigned int n_stellen) {
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

        namespace String {

            std::string trim(const std::string& str) {
                // Find the first non-whitespace character from the beginning
                size_t start = str.find_first_not_of(" \t\n\r");

                // If the string is all whitespace, return an empty string
                if (start == std::string::npos) return "";

                // Find the last non-whitespace character from the end
                size_t end = str.find_last_not_of(" \t\n\r");

                // Return the substring containing the trimmed characters
                return str.substr(start, end - start + 1);
            }

            std::string toLowercase(const std::string& str) {
                std::string result = str;
                std::transform(result.begin(), result.end(), result.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return result;
            }

            std::string replaceLineBreaksWithSpace(const std::string& input) {
                std::string result = input;
                std::replace(result.begin(), result.end(), '\n', ' ');
                return result;
            }

            bool includesString(const std::string& input, const std::vector<std::string>& strings) {
                std::string lowerInput = toLowercase(input);

                for (const auto& str : strings) {
                    std::string lowerStr = str;
                    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (lowerInput.find(lowerStr) != std::string::npos) {
                        return true;  // The input string includes one of the strings
                    }
                }
                return false;  // None of the strings in 'strings' were found in the input string
            }

            // if it includes any of the words in the vector
            bool includesWord(const std::string& input, const std::vector<std::string>& strings) {
                std::string lowerInput = toLowercase(input);
                lowerInput = replaceLineBreaksWithSpace(lowerInput);
                lowerInput = trim(lowerInput);
                lowerInput = " " + lowerInput + " ";  // Add spaces to the beginning and end of the string

                for (const auto& str : strings) {
                    std::string lowerStr = str;
                    lowerStr = trim(lowerStr);
                    lowerStr = " " + lowerStr + " ";  // Add spaces to the beginning and end of the string
                    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                    // logger::trace("lowerInput: {} lowerStr: {}", lowerInput, lowerStr);

                    if (lowerInput.find(lowerStr) != std::string::npos) {
                        return true;  // The input string includes one of the strings
                    }
                }
                return false;  // None of the strings in 'strings' were found in the input string
            }

            std::vector<std::pair<int, bool>> encodeString(const std::string& inputString) {
                std::vector<std::pair<int, bool>> encodedValues;
                try {
                    for (int i = 0; i < 100 && inputString[i] != '\0'; i++) {
                        char ch = inputString[i];
                        if (std::isprint(ch) && (std::isalnum(ch) || std::isspace(ch) ||
                            std::ispunct(ch)) && ch >= 0 && ch <= 255) {
                            encodedValues.push_back(std::make_pair(static_cast<int>(ch), std::isupper(ch)));
                        }
                    }
                } catch (const std::exception& e) {
                    logger::error("Error encoding string: {}", e.what());
                    return encodeString("ERROR");
                }
                return encodedValues;
            }

            std::string decodeString(const std::vector<std::pair<int, bool>>& encodedValues) {
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

        };

    };

    namespace Math {
        namespace LinAlg {
            namespace R3 {
                void rotateX(RE::NiPoint3& v, float angle) {
                    float y = v.y * cos(angle) - v.z * sin(angle);
                    float z = v.y * sin(angle) + v.z * cos(angle);
                    v.y = y;
                    v.z = z;
                }

                // Function to rotate a vector around the y-axis
                void rotateY(RE::NiPoint3& v, float angle) {
                    float x = v.x * cos(angle) + v.z * sin(angle);
                    float z = -v.x * sin(angle) + v.z * cos(angle);
                    v.x = x;
                    v.z = z;
                }

                // Function to rotate a vector around the z-axis
                void rotateZ(RE::NiPoint3& v, float angle) {
                    float x = v.x * cos(angle) - v.y * sin(angle);
                    float y = v.x * sin(angle) + v.y * cos(angle);
                    v.x = x;
                    v.y = y;
                }

                void rotate(RE::NiPoint3& v, float angleX, float angleY, float angleZ) {
                    rotateX(v, angleX);
                    rotateY(v, angleY);
                    rotateZ(v, angleZ);
                }
            };
        };
    };

    namespace FunctionsSkyrim {
    
        RE::TESForm* GetFormByID(const RE::FormID& id, const std::string& editor_id) {
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
                        inventory_changes->SetFavorite((*it), nullptr);
                    } else {
                        logger::trace("ExtraLists found");
                        inventory_changes->SetFavorite((*it), xLists->front());
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

        namespace WorldObject {

            float GetDistanceFromPlayer(RE::TESObjectREFR* ref) {
                if (!ref) {
                    logger::error("Ref is null.");
                    return 0;
                }
                auto player_pos = RE::PlayerCharacter::GetSingleton()->GetPosition();
                auto ref_pos = ref->GetPosition();
                return player_pos.GetDistance(ref_pos);
            }

            void SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok=true) {
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
                if (ref_base->GetFormID() == a_to->GetFormID()) {
				    logger::trace("Ref and base are the same.");
				    return;
			    }
                a_from->SetObjectReference(a_to);
                a_from->Disable();
                a_from->Enable(false);
                if (!apply_havok) return;

                /*float afX = 100;
                float afY = 100;
                float afZ = 100;
                float afMagnitude = 100;*/
                /*auto args = RE::MakeFunctionArguments(std::move(afX), std::move(afY), std::move(afZ),
                std::move(afMagnitude)); vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback);*/
                // Looked up here (wSkeever): https:  // www.nexusmods.com/skyrimspecialedition/mods/73607
                SKSE::GetTaskInterface()->AddTask([a_from]() {
                    // auto player_ch = RE::PlayerCharacter::GetSingleton();
                    // player_ch->StartGrabObject();
                    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    auto policy = vm->GetObjectHandlePolicy();
                    auto handle = policy->GetHandleForObject(a_from->GetFormType(), a_from);
                    RE::BSTSmartPointer<RE::BSScript::Object> object = nullptr;
                    vm->CreateObject2("ObjectReference", object);
                    vm->BindObject(object, handle, false);
                    if (!object) logger::warn("Object is null");
                    auto args = RE::MakeFunctionArguments(std::move(0.f), std::move(0.f), std::move(0.f), std::move(0.f));
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                    if (vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback)) logger::trace("FUSRODAH");
                });
            }

        };
    };

    namespace Types {

        // using EditorID = std::string;
        using NameID = std::string;
        using FormID = RE::FormID;
        using RefID = std::uint32_t;

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
            FormID id;         // fake formid
            bool equipped;     // is equipped
            bool favorited;    // is favorited
            RefID refid;       // refid of unowned/realoutintheworld/externalcont

            SaveDataRHS2() : id(0), equipped(false), favorited(false), refid(0) {}

        };

    }

    bool read_string(SKSE::SerializationInterface* a_intfc, std::string& a_str) {
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

    bool write_string(SKSE::SerializationInterface* a_intfc, const std::string& a_str) {
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

        [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface, const bool) override {
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

                m_Data[formId] = value;
                logger::trace("Loaded data for FormRefID: ({},{})", formId.outerKey, formId.innerKey);
            }
            return true;
        }
    };


};