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

    inline bool EqStr(const char* str1, const char* str2) { return std::strcmp(str1, str2) == 0; }

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

        bool isValidHexWithLength7or8(const char* input) {
            std::string inputStr(input);

            if (inputStr.substr(0, 2) == "0x") {
                // Remove "0x" from the beginning of the string
                inputStr = inputStr.substr(2);
            }

            std::regex hexRegex("^[0-9A-Fa-f]{7,8}$");  // Allow 7 to 8 characters
            bool isValid = std::regex_match(inputStr, hexRegex);
            return isValid;
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
    
        RE::TESForm* GetFormByID(const RE::FormID& id, const std::string& editor_id = "") {
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

        const std::string GetEditorID(const FormID a_formid) {
            if (const auto form = RE::TESForm::LookupByID(a_formid)) {
                return clib_util::editorID::get_editorID(form);
            } else {
                return "";
            }
        }

        FormID GetFormEditorIDFromString(const std::string& formEditorId) {
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
            if (formEditorId.empty())
                return 0;
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

        namespace xData {

            void AddTextDisplayData(RE::ExtraDataList* extraDataList, const std::string& displayName) {
                if (!extraDataList) return;
                if (extraDataList->HasType(RE::ExtraDataType::kTextDisplayData)) return;
				auto textDisplayData = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
                textDisplayData->SetName(displayName.c_str()); 
				extraDataList->Add(textDisplayData);
			}

            namespace Copy {
                void CopyEnchantment(RE::ExtraEnchantment* from, RE::ExtraEnchantment* to) {
                    logger::trace("CopyEnchantment");
                    to->enchantment = from->enchantment;
                    to->charge = from->charge;
                    to->removeOnUnequip = from->removeOnUnequip;
                }

                void CopyHealth(RE::ExtraHealth* from, RE::ExtraHealth* to) {
                    logger::trace("CopyHealth");
                    to->health = from->health;
                }

                void CopyRank(RE::ExtraRank* from, RE::ExtraRank* to) {
                    logger::trace("CopyRank");
                    to->rank = from->rank;
                }

                void CopyTimeLeft(RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to) {
                    logger::trace("CopyTimeLeft");
                    to->time = from->time;
                }

                void CopyCharge(RE::ExtraCharge* from, RE::ExtraCharge* to) {
                    logger::trace("CopyCharge");
                    to->charge = from->charge;
                }

                void CopyScale(RE::ExtraScale* from, RE::ExtraScale* to) {
                    logger::trace("CopyScale");
                    to->scale = from->scale;
                }

                void CopyUniqueID(RE::ExtraUniqueID* from, RE::ExtraUniqueID* to) {
                    logger::trace("CopyUniqueID");
                    to->baseID = from->baseID;
                    to->uniqueID = from->uniqueID;
                }

                void CopyPoison(RE::ExtraPoison* from, RE::ExtraPoison* to) {
                    logger::trace("CopyPoison");
                    to->poison = from->poison;
                    to->count = from->count;
                }

                void CopyObjectHealth(RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to) {
                    logger::trace("CopyObjectHealth");
                    to->health = from->health;
                }
                // *
                void CopyLight(RE::ExtraLight*, RE::ExtraLight*) {
                    logger::trace("CopyLight");
                    return;
                    //to->lightData = from->lightData;
                }

                void CopyRadius(RE::ExtraRadius* from, RE::ExtraRadius* to) {
                    logger::trace("CopyRadius");
                    to->radius = from->radius;
                }
                // *
                void CopyHorse(RE::ExtraHorse*, RE::ExtraHorse*) {
                    logger::trace("CopyHorse");
                    return;
                    //to->horseRef = from->horseRef;
                }

                void CopyHotkey(RE::ExtraHotkey* from, RE::ExtraHotkey* to) {
                    logger::trace("CopyHotkey");
                    to->hotkey = from->hotkey;
                }

                void CopyTextDisplayData(RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to) {
                    to->displayName = from->displayName;
                    to->displayNameText = from->displayNameText;
                    to->ownerQuest = from->ownerQuest;
                    to->ownerInstance = from->ownerInstance;
                    to->temperFactor = from->temperFactor;
                    to->customNameLength = from->customNameLength;
                }

                void CopySoul(RE::ExtraSoul* from, RE::ExtraSoul* to) {
                    logger::trace("CopySoul");
                    to->soul = from->soul;
                }

                void CopyOwnership(RE::ExtraOwnership* from, RE::ExtraOwnership* to) {
                    logger::trace("CopyOwnership");
                    to->owner = from->owner;
                }
            };

            template <typename T>
            void CopyExtraData(T* from, T* to) {
                if (!from || !to) return;
                switch (T->EXTRADATATYPE) {
                    case RE::ExtraDataType::kEnchantment:
                        CopyEnchantment(from, to);
                        break;
                    case RE::ExtraDataType::kHealth:
                        CopyHealth(from, to);
                        break;
                    case RE::ExtraDataType::kRank:
                        CopyRank(from, to);
                        break;
                    case RE::ExtraDataType::kTimeLeft:
                        CopyTimeLeft(from, to);
                        break;
                    case RE::ExtraDataType::kCharge:
                        CopyCharge(from, to);
                        break;
                    case RE::ExtraDataType::kScale:
                        CopyScale(from, to);
                        break;
                    case RE::ExtraDataType::kUniqueID:
                        CopyUniqueID(from, to);
                        break;
                    case RE::ExtraDataType::kPoison:
                        CopyPoison(from, to);
                        break;
                    case RE::ExtraDataType::kObjectHealth:
                        CopyObjectHealth(from, to);
                        break;
                    case RE::ExtraDataType::kLight:
                        CopyLight(from, to);
                        break;
                    case RE::ExtraDataType::kRadius:
                        CopyRadius(from, to);
                        break;
                    case RE::ExtraDataType::kHorse:
                        CopyHorse(from, to);
                        break;
                    case RE::ExtraDataType::kHotkey:
                        CopyHotkey(from, to);
                        break;
                    case RE::ExtraDataType::kTextDisplayData:
                        CopyTextDisplayData(from, to);
                        break;
                    case RE::ExtraDataType::kSoul:
                        CopySoul(from, to);
                        break;
                    case RE::ExtraDataType::kOwnership:
                        CopyOwnership(from, to);
                        break;
                    default:
                        logger::warn("ExtraData type not found");
                        break;
                };
            }

            [[nodiscard]] const bool UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to) {
                logger::trace("UpdateExtras");
                if (!copy_from || !copy_to) return false;
                // Enchantment
                if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
                    logger::trace("Enchantment found");
                    auto enchantment =
                        static_cast<RE::ExtraEnchantment*>(copy_from->GetByType(RE::ExtraDataType::kEnchantment));
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
                    auto objhealth =
                        static_cast<RE::ExtraObjectHealth*>(copy_from->GetByType(RE::ExtraDataType::kObjectHealth));
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
                    auto textdisplaydata = static_cast<RE::ExtraTextDisplayData*>(
                        copy_from->GetByType(RE::ExtraDataType::kTextDisplayData));
                    if (textdisplaydata) {
                        RE::ExtraTextDisplayData* textdisplaydata_fake =
                            RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
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
                    auto ownership =
                        static_cast<RE::ExtraOwnership*>(copy_from->GetByType(RE::ExtraDataType::kOwnership));
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

            [[nodiscard]] const bool UpdateExtras(RE::TESObjectREFR* copy_from, RE::TESObjectREFR* copy_to) {
                logger::trace("UpdateExtras");
                if (!copy_from || !copy_to) {
                    logger::error("copy_from or copy_to is null");
                    return false;
                }
                auto* copy_from_extralist = &copy_from->extraList;
                auto* copy_to_extralist = &copy_to->extraList;
                return UpdateExtras(copy_from_extralist, copy_to_extralist);
            }

            const int32_t GetXDataCostOverride(RE::ExtraDataList* xList) {
				if (!xList) return 0;
				int32_t extra_costs = 0;
                if (xList && GetExtraDataListLength(xList)) {
                    if (auto xench = xList->GetByType<RE::ExtraEnchantment>()){
                        auto ench = xench->enchantment;
                        auto temp_costoverride = ench->data.costOverride;
                        if (temp_costoverride < 0)
                            temp_costoverride = static_cast<int32_t>(ench->CalculateTotalGoldValue());
                        if (temp_costoverride < 0)
                            temp_costoverride = static_cast<int32_t>(
                                ench->CalculateTotalGoldValue(RE::PlayerCharacter::GetSingleton()));
                        if (temp_costoverride > 0) {
                            logger::trace("CostOverride: {}", temp_costoverride);
                            extra_costs += temp_costoverride;
                        }
                    }
                }
				return extra_costs;
			}

            void PrintObjectExtraData(RE::TESObjectREFR* ref) {
                if (!ref) {
                    logger::error("Ref is null.");
                    return;
                }
                logger::trace("printing ExtraDataList");
                for (int i = 0; i < 191; i++) {
                    if (ref->extraList.HasType(static_cast<RE::ExtraDataType>(i))) {
                        logger::trace("ExtraDataList type: {}", i);
                    }
                }
            }
            
        };

        namespace Menu {
            void RefreshInventoryMenu() {
                auto ui = RE::UI::GetSingleton();
                if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
                    auto invMenu = ui->GetMenu<RE::InventoryMenu>(RE::InventoryMenu::MENU_NAME);
                    invMenu.get()->GetRuntimeData().itemList->Update();
                }
            }

            // po3
            void SendInventoryUpdateMessage(RE::TESObjectREFR* a_inventoryRef, const RE::TESBoundObject* a_updateObj) {
                using func_t = decltype(&SendInventoryUpdateMessage);
                static REL::Relocation<func_t> func{RELOCATION_ID(51911, 52849)};
                return func(a_inventoryRef, a_updateObj);
            }
        };

        namespace Inventory {
            
            const bool EntryHasXData(RE::InventoryEntryData* entry) {
                if (entry && entry->extraLists && !entry->extraLists->empty()) return true;
                return false;
            }

            const int32_t GetEntryCostOverride(RE::InventoryEntryData* entry){ 
                if (!EntryHasXData(entry)) return 0;
                int32_t extra_costs = 0;
                for (auto& xList : *entry->extraLists) {
                    extra_costs += xData::GetXDataCostOverride(xList);
                }
                return extra_costs;
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


            const bool HasItemEntry(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner,
                                    bool nonzero_entry_check = false) {
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


            const std::int32_t GetItemValue(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
                if (HasItemEntry(item, inventory_owner, true)) {
                    return inventory_owner->GetInventory().find(item)->second.second->GetValue();
                }
                return 0;
            }

            const std::int32_t GetItemCount(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
                if (HasItemEntry(item, inventory_owner, true)) {
                    auto inventory = inventory_owner->GetInventory();
                    auto it = inventory.find(item);
                    return it->second.first;
                }
                return 0;
            }

            const bool HasItem(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {
                if (HasItemEntry(item, item_owner, true)) return true;
                return false;
            }

            void AddItem(RE::TESObjectREFR* addTo, RE::TESObjectREFR* addFrom, FormID item_id, Count count,
                         RE::ExtraDataList* xList = nullptr) {
                logger::trace("AddItem");
                // xList = nullptr;
                if (!addTo) {
                    logger::error("add to is null!");
                    return;
                }

                logger::trace("Adding item.");

                auto bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item_id);
                addTo->AddObjectToContainer(bound, xList, count, addFrom);
            }

            void RemoveAll(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner) {
                if (!item) return;
                if (!item_owner) return;
                auto inventory = item_owner->GetInventory();
                auto it = inventory.find(item);
                bool has_entry = it != inventory.end();
                if (!has_entry) return;
                item_owner->RemoveItem(item, std::min(it->second.first, 1), RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                                       nullptr);
            }

            void FavoriteItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
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

            void FavoriteItem(const FormID formid, const FormID refid) {
                FavoriteItem(GetFormByID<RE::TESBoundObject>(formid), GetFormByID<RE::TESObjectREFR>(refid));
            }

            [[nodiscard]] const bool HasItemPlusCleanUp(RE::TESBoundObject* item, RE::TESObjectREFR* item_owner,
                                                        RE::ExtraDataList* xList = nullptr) {
                logger::trace("HasItemPlusCleanUp");

                if (HasItem(item, item_owner)) return true;
                if (HasItemEntry(item, item_owner)) {
                    item_owner->RemoveItem(item, 1, RE::ITEM_REMOVE_REASON::kRemove, xList, nullptr);
                    logger::trace("Item with zero count removed from player.");
                }
                return false;
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

            [[nodiscard]] const bool IsFavorited(RE::FormID formid, RE::FormID refid) {
                return IsFavorited(GetFormByID<RE::TESBoundObject>(formid), GetFormByID<RE::TESObjectREFR>(refid));
            }

            [[nodiscard]] const bool IsPlayerFavorited(RE::TESBoundObject* item) {
                return IsFavorited(item, RE::PlayerCharacter::GetSingleton()->AsReference());
            }

            [[nodiscard]] const bool IsEquipped(RE::TESBoundObject* item) {
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

            [[nodiscard]] const bool IsEquipped(const FormID formid) {
                return IsEquipped(GetFormByID<RE::TESBoundObject>(formid));
            }

            void EquipItem(RE::TESBoundObject* item, bool unequip = false) {
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
                                    player_ref, (*it)->object, nullptr, 1, (const RE::BGSEquipSlot*)nullptr, true,
                                    false, false);
                            } else if ((*it)->extraLists->front()) {
                                RE::ActorEquipManager::GetSingleton()->UnequipObject(
                                    player_ref, (*it)->object, (*it)->extraLists->front(), 1,
                                    (const RE::BGSEquipSlot*)nullptr, true, false, false);
                            }
                        } else {
                            if ((*it)->extraLists->empty()) {
                                RE::ActorEquipManager::GetSingleton()->EquipObject(player_ref, (*it)->object, nullptr,
                                                                                   1, (const RE::BGSEquipSlot*)nullptr,
                                                                                   true, false, false, false);
                            } else if ((*it)->extraLists->front()) {
                                RE::ActorEquipManager::GetSingleton()->EquipObject(
                                    player_ref, (*it)->object, (*it)->extraLists->front(), 1,
                                    (const RE::BGSEquipSlot*)nullptr, true, false, false, false);
                            }
                        }
                        return;
                    }
                }
            }

            void EquipItem(const FormID formid, bool unequip = false) {
                EquipItem(GetFormByID<RE::TESBoundObject>(formid), unequip);
            }
        };


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

            void SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok = true) {
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

            void SetObjectCount(RE::TESObjectREFR* ref, Count count) {
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

            RE::TESObjectREFR* DropObjectIntoTheWorld(RE::TESBoundObject* obj, Count count=1, bool owned = true) {
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
                                      ->CreateReferenceAtLocation(obj, drop_pos, {0.0f, 0.0f, 0.0f}, player_cell,
                                                                  player_ws, nullptr, nullptr, {}, false, false)
                                      .get()
                                      .get();
                if (!newPropRef) {
                    logger::critical("New prop ref is null.");
                    return nullptr;
                }
                if (count>1) SetObjectCount(newPropRef, count);
                if (owned) newPropRef->extraList.SetOwner(RE::TESForm::LookupByID<RE::TESForm>(0x07));
                return newPropRef;
            }

        };
        

        // SkyrimThiago <3
        // https://github.com/Thiago099/DPF-Dynamic-Persistent-Forms
        namespace DynamicForm {

            const bool IsDynamicFormID(const FormID a_formID) { return a_formID >= 0xFF000000; }

            static void copyBookAppearence(RE::TESForm* source, RE::TESForm* target) {
                auto* sourceBook = source->As<RE::TESObjectBOOK>();

                auto* targetBook = target->As<RE::TESObjectBOOK>();

                if (sourceBook && targetBook) {
                    targetBook->inventoryModel = sourceBook->inventoryModel;
                }
            }

            template <class T>

            void copyComponent(RE::TESForm* from, RE::TESForm* to) {
                auto fromT = from->As<T>();

                auto toT = to->As<T>();

                if (fromT && toT) {
                    toT->CopyComponent(fromT);
                }
            }

            static void copyFormArmorModel(RE::TESForm* source, RE::TESForm* target) {
                auto* sourceModelBipedForm = source->As<RE::TESObjectARMO>();

                auto* targeteModelBipedForm = target->As<RE::TESObjectARMO>();

                if (sourceModelBipedForm && targeteModelBipedForm) {
                    logger::info("armor");

                    targeteModelBipedForm->armorAddons = sourceModelBipedForm->armorAddons;
                }
            }

            static void copyFormObjectWeaponModel(RE::TESForm* source, RE::TESForm* target) {
                auto* sourceModelWeapon = source->As<RE::TESObjectWEAP>();

                auto* targeteModelWeapon = target->As<RE::TESObjectWEAP>();

                if (sourceModelWeapon && targeteModelWeapon) {
                    logger::info("weapon");

                    targeteModelWeapon->firstPersonModelObject = sourceModelWeapon->firstPersonModelObject;

                    targeteModelWeapon->attackSound = sourceModelWeapon->attackSound;

                    targeteModelWeapon->attackSound2D = sourceModelWeapon->attackSound2D;

                    targeteModelWeapon->attackSound = sourceModelWeapon->attackSound;

                    targeteModelWeapon->attackFailSound = sourceModelWeapon->attackFailSound;

                    targeteModelWeapon->idleSound = sourceModelWeapon->idleSound;

                    targeteModelWeapon->equipSound = sourceModelWeapon->equipSound;

                    targeteModelWeapon->unequipSound = sourceModelWeapon->unequipSound;

                    targeteModelWeapon->soundLevel = sourceModelWeapon->soundLevel;
                }
            }

            static void copyMagicEffect(RE::TESForm* source, RE::TESForm* target) {
                auto* sourceEffect = source->As<RE::EffectSetting>();

                auto* targetEffect = target->As<RE::EffectSetting>();

                if (sourceEffect && targetEffect) {
                    targetEffect->effectSounds = sourceEffect->effectSounds;

                    targetEffect->data.castingArt = sourceEffect->data.castingArt;

                    targetEffect->data.light = sourceEffect->data.light;

                    targetEffect->data.hitEffectArt = sourceEffect->data.hitEffectArt;

                    targetEffect->data.effectShader = sourceEffect->data.effectShader;

                    targetEffect->data.hitVisuals = sourceEffect->data.hitVisuals;

                    targetEffect->data.enchantShader = sourceEffect->data.enchantShader;

                    targetEffect->data.enchantEffectArt = sourceEffect->data.enchantEffectArt;

                    targetEffect->data.enchantVisuals = sourceEffect->data.enchantVisuals;

                    targetEffect->data.projectileBase = sourceEffect->data.projectileBase;

                    targetEffect->data.explosion = sourceEffect->data.explosion;

                    targetEffect->data.impactDataSet = sourceEffect->data.impactDataSet;

                    targetEffect->data.imageSpaceMod = sourceEffect->data.imageSpaceMod;
                }
            }

            void copyAppearence(RE::TESForm* source, RE::TESForm* target) {
                copyFormArmorModel(source, target);

                copyFormObjectWeaponModel(source, target);

                copyMagicEffect(source, target);

                copyBookAppearence(source, target);

                copyComponent<RE::BGSPickupPutdownSounds>(source, target);

                copyComponent<RE::BGSMenuDisplayObject>(source, target);

                copyComponent<RE::TESModel>(source, target);

                copyComponent<RE::TESBipedModelForm>(source, target);
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