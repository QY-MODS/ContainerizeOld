#include "Manager.h"

// FFI04Sack 000DAB04

Manager* M;


class OurEventSink : public RE::BSTEventSink<RE::TESActivateEvent>,
                     public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                     public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
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
    // (WIP)
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) {

        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
        if (!event->actionRef->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        if (!M->RefIsContainer(event->objectActivated.get())) return RE::BSEventNotifyControl::kContinue;
        
        
        M->ActivateContainer(event->objectActivated.get());


        return RE::BSEventNotifyControl::kContinue;
    }

    // to disable ref activation (DONE)
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {

        if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
        if (event->crosshairRef->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;

        //logger::info("Crosshair is over {}", event->crosshairRef->GetBaseObject()->GetName());
        
        if (M->RefIsContainer(event->crosshairRef.get())) {
			event->crosshairRef->SetActivationBlocked(1);
            logger::info("Ref activation disabled");
		}
        return RE::BSEventNotifyControl::kContinue;
    }
    // (MAYBE)
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
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
            logger::info("Pressed key {}", dxScanCode);
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
    }
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();

    SKSE::Init(skse);
    auto* eventSink = OurEventSink::GetSingleton();

    // ScriptSource
    auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);

    // SKSE
    SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);

    // UI
    //RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(eventSink);

    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}