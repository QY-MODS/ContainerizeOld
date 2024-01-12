#pragma once

//#include <chrono>
#include "logger.h"
#include <windows.h>
#include <functional>

namespace Utilities {

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
    /*const auto load_order_msgbox = std::format(
        "The equipped light source from your save game could not be registered. Please unequip and reequip it. If you "
        "had fuel in it, it will be lost. This issue will be solved in the next version.");*/

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

            void InitErr() { RE::DebugMessageBox(init_err_msgbox.c_str()); };

            void GeneralErr() { RE::DebugMessageBox(general_err_msgbox.c_str()); };

            void NoSourceFound() { RE::DebugMessageBox(no_src_msgbox.c_str()); };

            void FormIDError(RE::FormID id) {
                RE::DebugMessageBox(
                    std::format("{}: The ID ({}) you have provided in the ini file could not have been found.",
                                Utilities::mod_name, Utilities::dec2hex(id))
                        .c_str());
            }

            void EditorIDError(std::string id) {
                RE::DebugMessageBox(
                    std::format("{}: The ID ({}) you have provided in the ini file could not have been found.",
                                Utilities::mod_name, id)
                        .c_str());
            }

            /*void LoadOrderError() {
                RE::DebugMessageBox((std::format("{}: ", Utilities::mod_name) + load_order_msgbox).c_str());
            }*/
        };
    };

    // Custom struct to represent the combined data
    // butun RE::FormID'ler bununla takas edildi
    struct FormID2 {
        uint32_t outerKey;
        uint32_t innerKey;

        // Comparison operator for using Key as std::map key
        bool operator<(const FormID2& other) const {
            return outerKey < other.outerKey || (outerKey == other.outerKey && innerKey < other.innerKey);
        }
    };

    // https :  // github.com/ozooma10/OSLAroused/blob/29ac62f220fadc63c829f6933e04be429d4f96b0/src/PersistedData.cpp
    template <typename T>
    // BaseData is based off how powerof3's did it in Afterlife
    class BaseData {
    public:
        float GetData(FormID2 formId, T missing) {
            Locker locker(m_Lock);
            if (auto idx = m_Data.find(formId) != m_Data.end()) {
                return m_Data[formId];
            }
            return missing;
        }

        void SetData(FormID2 formId, T value) {
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
        std::map<FormID2, T> m_Data;

        using Lock = std::recursive_mutex;
        using Locker = std::lock_guard<Lock>;
        mutable Lock m_Lock;
    };

    class BaseFormFloat : public BaseData<float> {
    public:
        virtual void DumpToLog() override {
            Locker locker(m_Lock);
            for (const auto& [formId, value] : m_Data) {
                logger::info("Dump Row From {} - FormID1st: {} - FormID2nd: {} - value: {}", GetType(), formId.outerKey,
                             formId.innerKey, value);
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
                logger::error("Failed to save data for FormID2: ({},{})", formId.outerKey, formId.innerKey);
                return false;
            }

            if (!serializationInterface->WriteRecordData(value)) {
                logger::error("Failed to save value data for form2: ({},{})", formId.outerKey, formId.innerKey);
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

        FormID2 formId;
        T value;

        for (auto i = 0; i < recordDataSize; i++) {
            serializationInterface->ReadRecordData(formId);
            // Ensure form still exists
            // bunu nasil yapacagiz?
            FormID2 fixedId;
            if (!serializationInterface->ResolveFormID(formId.outerKey, fixedId.outerKey) ||
                !serializationInterface->ResolveFormID(formId.innerKey, fixedId.innerKey)) {
                logger::error("Failed to resolve formID1st {} {} and Failed to resolve formID2nd {} {}"sv,
                              formId.outerKey, fixedId.outerKey, formId.innerKey, fixedId.innerKey);
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