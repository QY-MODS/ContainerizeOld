#pragma once

//#include <chrono>
#include "logger.h"
#include <windows.h>
#include <functional>
#include <unordered_set>
#include "SimpleIni.h"
#include <iostream>
#include <string>
#include <codecvt>

//using _GetFormEditorID = const char* (*)(std::uint32_t);

namespace Utilities {

    // stuff
    const auto mod_name = static_cast<std::string>(SKSE::PluginDeclaration::GetSingleton()->GetName());
    constexpr auto po3path = "Data/SKSE/Plugins/po3_Tweaks.dll";

    const auto no_src_msgbox = std::format(
        "{}: You currently do not have any container set up. Check your ini file or see the mod page for instructions.",
        mod_name);
    /*const auto po3_err_msgbox = std::format(
        "{}: You have given an invalid FormID. If you are using Editor IDs, you must have powerofthree's Tweaks "
        "installed. See mod page for further instructions.",
        mod_name);*/
    const auto po3_err_msgbox = std::format(
        "{}: You have given an invalid FormID. If you are using Editor IDs, you must have powerofthree's Tweaks "
        "installed. See mod page for further instructions.",
        mod_name);
    const auto general_err_msgbox = std::format("{}: Something went wrong. Please contact the mod author.", mod_name);
    const auto init_err_msgbox = std::format("{}: The mod failed to initialize and will be terminated.", mod_name);
    /*const auto load_order_msgbox = std::format(
        "The equipped light source from your save game could not be registered. Please unequip and reequip it. If you "
        "had fuel in it, it will be lost. This issue will be solved in the next version.");*/

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
                for (auto text : buttonTextValues) messagebox->buttonText.push_back(text.c_str());
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
					std::format("{}: Problem with one of the items with the form id ({}). \
                        If you have changed the list of containers in the INI file between saves, please first get your items from your old containers.",
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

            /*void LoadOrderError() {
                RE::DebugMessageBox((std::format("{}: ", Utilities::mod_name) + load_order_msgbox).c_str());
            }*/
        };
    };

    namespace Types {

        //using EditorID = std::string;
        using NameID = std::string;
        using RefID = std::uint32_t;
        using FormID = std::uint32_t;

        struct FormRefID {
            FormID outerKey;
            RefID innerKey;

            bool operator<(const FormRefID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };

        /*struct EditorRefID {
            EditorID outerKey;
            RefID innerKey;

            bool operator<(const EditorRefID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };

        struct FormEditorID {
            FormID outerKey;
            EditorID innerKey;

            bool operator<(const FormEditorID& other) const {
                return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
            }
        };*/

        //struct EditorNameID {
        //    EditorID outerKey;
        //    NameID innerKey;

        //    bool operator<(const EditorNameID& other) const {
        //        return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
        //    }
        //};

        using SourceDataKey = RefID;
        using SourceDataVal = RefID;
        using SourceData = std::map<SourceDataKey, SourceDataVal>; // Container-Chest Reference ID Pairs
    
    }
    
    // Get ID stuff
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

    // https:// github.com/powerof3/AnimObjectSwapper/blob/9b4ec05b87ec35031bfd337e3d9786bc36139a83/src/Manager.cpp#L57
    //Types::EditorID GetEditorID(const RE::TESForm* a_form) {
    //    switch (a_form->GetFormType()) {
    //        case RE::FormType::Keyword:
    //        case RE::FormType::LocationRefType:
    //        case RE::FormType::Action:
    //        case RE::FormType::MenuIcon:
    //        case RE::FormType::Global:
    //        case RE::FormType::HeadPart:
    //        case RE::FormType::Race:
    //        case RE::FormType::Sound:
    //        case RE::FormType::Script:
    //        case RE::FormType::Navigation:
    //        case RE::FormType::Cell:
    //        case RE::FormType::WorldSpace:
    //        case RE::FormType::Land:
    //        case RE::FormType::NavMesh:
    //        case RE::FormType::Dialogue:
    //        case RE::FormType::Quest:
    //        case RE::FormType::Idle:
    //        case RE::FormType::AnimatedObject:
    //        case RE::FormType::ImageAdapter:
    //        case RE::FormType::VoiceType:
    //        case RE::FormType::Ragdoll:
    //        case RE::FormType::DefaultObject:
    //        case RE::FormType::MusicType:
    //        case RE::FormType::StoryManagerBranchNode:
    //        case RE::FormType::StoryManagerQuestNode:
    //        case RE::FormType::StoryManagerEventNode:
    //        case RE::FormType::SoundRecord:
    //            return a_form->GetFormEditorID();
    //        default: {
    //            static auto tweaks = GetModuleHandle(L"po3_Tweaks");
    //            static auto func = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));
    //            if (func) {
    //                return func(a_form->formID);
    //            }
    //            return std::string();
    //        }
    //    }
    //}

    template <class T>
    uint32_t GetLength(T list) {
        logger::info("Getting length of list");
        uint32_t length = 0;
        for (auto _ : list) {
            ++length;
            logger::info("Length: {}", length);
        }
        return length;
    }


    template <class T>
    uint32_t GetListLength(RE::BSSimpleList<T>* list) {
        return GetLength(list);
    }

    template <class T>
    bool ValueExists(const std::map<T, T>& myMap, const T& searchValue) {
        bool valueExists = false;
        for (const auto& pair : myMap) {
            if (pair.second == searchValue) {
                // Value found
                valueExists = true;
                break;
            }
        }
        return valueExists;
    }

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

        Locker locker(m_Lock);
        m_Data.clear();

        Types::FormRefID formId;
        T value;

        for (auto i = 0; i < recordDataSize; i++) {
            serializationInterface->ReadRecordData(formId);
            // Ensure form still exists
            // bunu nasil yapacagiz?

            if (!serializationInterface->ResolveFormID(formId.outerKey, formId.outerKey)) {
                logger::error("Failed to resolve form ID, 0x{:X}.", formId.outerKey);
                continue;
            }

            serializationInterface->ReadRecordData(value);
            m_Data[formId] = value;
        }
        return true;
    }

    template <typename T>
    void BaseData<T>::Clear() {
        Locker locker(m_Lock);
        m_Data.clear();
    }

};