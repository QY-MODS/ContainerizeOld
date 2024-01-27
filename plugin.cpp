#include "Manager.h"

// FFI04Sack 000DAB04

Manager* M;
bool listen_container_changed = false;

class OurEventSink : public RE::BSTEventSink<RE::TESActivateEvent>,
                     public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                     public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                     public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                     public RE::BSTEventSink<RE::InputEvent*> {

    OurEventSink() = default;
    OurEventSink(const OurEventSink&) = delete;
    OurEventSink(OurEventSink&&) = delete;
    OurEventSink& operator=(const OurEventSink&) = delete;
    OurEventSink& operator=(OurEventSink&&) = delete;

public:
    static OurEventSink* GetSingleton() {
        static OurEventSink singleton;
        return &singleton;
    }
    // to put saved items to chest and to open chest (DONE)
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
        if (!event->actionRef->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        if (!event->objectActivated->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;
        if (event->objectActivated == RE::PlayerCharacter::GetSingleton()->GetGrabbedRef()) return RE::BSEventNotifyControl::kContinue;
        if (!M->listen_activate) return RE::BSEventNotifyControl::kContinue;
        if (!M->RefIsContainer(event->objectActivated.get())) return RE::BSEventNotifyControl::kContinue;
        
        //logger::info("container has {} extra data", extradatalist.size());
  //      for (auto it = extradatalist.begin(); it != extradatalist.end(); ++it) {
		//	it->GetType();
  // //         if (extra->GetType() == RE::ExtraDataType::kContainerChanges) {
		//	//	auto containerchanges = static_cast<RE::ExtraContainerChanges*>(extra);
		//	//	auto changes = containerchanges->changes;
  // //             for (auto it2 = changes.begin(); it2 != changes.end(); ++it2) {
		//	//		auto change = *it2;
		//	//		auto item = change->item;
		//	//		auto count = change->countDelta;
		//	//		logger::info("Item {} count {}", item->GetName(), count);
		//	//		M->AddItemToContainer(item, count);
		//	//	}
		//	//}
		//}
        
        
        M->ActivateContainer(event->objectActivated.get());


        return RE::BSEventNotifyControl::kContinue;
    }

    // to disable ref activation (DONE)
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {

        if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
        if (event->crosshairRef->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;
        if (event->crosshairRef->extraList.GetCount()>1) return RE::BSEventNotifyControl::kContinue;

        //logger::info("Crosshair is over {}", event->crosshairRef->GetBaseObject()->GetName());
        
        if (M->RefIsContainer(event->crosshairRef.get())) {
			event->crosshairRef->SetActivationBlocked(1);
            //logger::info("Ref activation disabled");
		}
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // to close chest and save the contents and remove items (MAYBE)
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (event->menuName != "ContainerMenu") return RE::BSEventNotifyControl::kContinue;
        if (!M->listen_menuclose) return RE::BSEventNotifyControl::kContinue;
        //logger::info("Menu {} {}", event->menuName, event->opening);

        if (event->opening) {
            listen_container_changed = true;
        } else {
			//logger::info("Container menu closed");
            listen_container_changed = false;
			M->listen_menuclose = false;
            //M->DeactivateContainer();
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
                                                                   RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!listen_container_changed) return RE::BSEventNotifyControl::kContinue;
        if (event->oldContainer != 20) return RE::BSEventNotifyControl::kContinue;
        logger::info("Item {} went into container {}.", event->baseObj, event->newContainer);
        M->InspectItemTransfer();
        return RE::BSEventNotifyControl::kContinue;
    }


    // (MAYBE)
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* eventPtr, RE::BSTEventSource<RE::InputEvent*>*) {
        if (!eventPtr) return RE::BSEventNotifyControl::kContinue;

        auto* event = *eventPtr;
        if (!event) return RE::BSEventNotifyControl::kContinue;

        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
            auto* buttonEvent = event->AsButtonEvent();
            auto dxScanCode = buttonEvent->GetIDCode();
            //logger::info("Pressed key {}", dxScanCode);
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};


void OnMessage(SKSE::MessagingInterface::Message* message) {
    /*if (message->type == SKSE::MessagingInterface::kInputLoaded)
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(OurEventSink::GetSingleton());*/
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        // Start
        auto sources = Settings::LoadINISettings();
        M = Manager::GetSingleton(sources);
        
        // EventSink
        auto* eventSink = OurEventSink::GetSingleton();
        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();

        eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESContainerChangedEvent>(eventSink);
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(OurEventSink::GetSingleton());
        SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);
    }
}

void SaveCallback(SKSE::SerializationInterface* serializationInterface) {
    M->SendData();
    if (!M->Save(serializationInterface, Settings::kDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
}

void LoadCallback(SKSE::SerializationInterface* serializationInterface) {
    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t length;
    while (serializationInterface->GetNextRecordInfo(type, version, length)) {
        auto temp = Utilities::DecodeTypeCode(type);

        if (version != Settings::kSerializationVersion) {
            logger::critical("Loaded data has incorrect version. Recieved ({}) - Expected ({}) for Data Key ({})",
                             version, Settings::kSerializationVersion, temp);
            continue;
        }
        switch (type) {
            case Settings::kDataKey: {
                if (!M->Load(serializationInterface)) {
                    logger::critical("Failed to Load Data");
                }
            } break;
            default:
                logger::critical("Unrecognized Record Type: {}", temp);
                break;
        }
    }
    M->ReceiveData();
    logger::info("Data loaded from skse co-save.");
}

void InitializeSerialization() {
    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Settings::kDataKey);
    serialization->SetSaveCallback(SaveCallback);
    // serialization->SetRevertCallback(LightSourceManager::RevertCallback);
    serialization->SetLoadCallback(LoadCallback);
    SKSE::log::trace("Cosave serialization initialized.");
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    SKSE::Init(skse);
    InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}