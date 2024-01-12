#include "Manager.h"

bool EqStr(const char* str1, const char* str2) { return std::strcmp(str1, str2) == 0; }

class OurEventSink : public RE::BSTEventSink<RE::TESHitEvent>,
                     public RE::BSTEventSink<RE::TESActivateEvent>,
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

    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) {
        auto targetName = event->target->GetBaseObject()->GetName();
        auto sourceName = event->cause->GetBaseObject()->GetName();
        logger::info("{} hit {}", sourceName, targetName);
        if (event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) logger::info("Ooooo power attack!");
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) {
        auto activatedName = event->objectActivated->GetBaseObject()->GetName();
        /*if (EqStr(activatedName,"Unowned Chest")) {
            logger::info("Unowned Chest is activatable: {}", event->objectActivated->IsActivationBlocked());
		}*/
        auto activatorName = event->actionRef->GetBaseObject()->GetName();
        logger::info("{} activated {}", activatorName, activatedName);
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
        if (event->crosshairRef) {
            logger::info("Crosshair is over {}", event->crosshairRef->GetBaseObject()->GetName());
        }
        /*if (EqStr(event->crosshairRef->GetBaseObject()->GetName(), "Unowned Chest")) {
            event->crosshairRef->SetActivationBlocked(1);
        }*/
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (event->opening)
            logger::info("OPEN MENU {}", event->menuName);
        else
            logger::info("CLOSE MENU {}", event->menuName);
        return RE::BSEventNotifyControl::kContinue;
    }

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
    if (message->type == SKSE::MessagingInterface::kInputLoaded)
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(OurEventSink::GetSingleton());
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();

    SKSE::Init(skse);
    auto* eventSink = OurEventSink::GetSingleton();

    // ScriptSource
    auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESHitEvent>(eventSink);
    eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);

    // SKSE
    SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);

    // UI
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(eventSink);

    // Input Device
    //SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}