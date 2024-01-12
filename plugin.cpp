#include "Manager.h"


RE::TESObjectCONT* unownedChest = nullptr;
RE::TESObjectREFR* unownedChestObjRefr = nullptr;
RE::TESObjectCELL* unownedCell = nullptr;
RE::NiPoint3 unownedChestPos = { 1989.0740f, 1793.2015f, 6784.0000f };
uint32_t total_chests = 1;

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
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
        auto activatedName = event->objectActivated->GetBaseObject()->GetName();
        if (EqStr(activatedName,"Unowned Chest")) {
            logger::info("Unowned Chest is not activatable: {}", event->objectActivated->IsActivationBlocked());
            auto item = event->objectActivated->GetBaseObject();
            /*unownedChest->AddObjectToContainer(item,10,RE::PlayerCharacter::GetSingleton()->As<RE::TESForm>());
            unownedChest->fullName = "lol";
            unownedChest->Activate(event->objectActivated.get(), RE::PlayerCharacter::GetSingleton(), 0, item, 10);*/
            total_chests += 1;
            logger::info("asd");
            logger::info("total_chests {}", total_chests);
            int total_chests_x = ((((int)total_chests - 1) + 2) % 5) - 2;
            logger::info("total_chests_x: {}", total_chests_x);
            //unownedChestPos.y += 50.0f * ((total_chests - 1));
            float Pos3_x = unownedChestPos.x + 100.0f * total_chests_x;
            RE::NiPoint3 Pos3 = { Pos3_x, unownedChestPos.y, unownedChestPos.z };
            logger::info("Pos3: {}, {}, {}", Pos3.x, Pos3.y, Pos3.z);
            auto newPropRef = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(item, Pos3,
                                                                                  {0.0f, 0.0f, 0.0f},
                                                              unownedCell, nullptr, nullptr, nullptr,
                                                              {}, true, false).get().get();
            logger::info("Created Object! Type: {}, Base ID: {:x}, Ref ID: {:x},",
                         RE::FormTypeToString(item->GetFormType()), item->GetFormID(), newPropRef->GetFormID());
		}
        auto activatorName = event->actionRef->GetBaseObject()->GetName();
        logger::info("{} activated {}", activatorName, activatedName);
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
        if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
        logger::info("Crosshair is over {}", event->crosshairRef->GetBaseObject()->GetName());
        if (EqStr(event->crosshairRef->GetBaseObject()->GetName(), "Unowned Chest")) {
            event->crosshairRef->SetActivationBlocked(1);
        }
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
    /*if (message->type == SKSE::MessagingInterface::kInputLoaded)
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(OurEventSink::GetSingleton());*/
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        unownedChest = RE::TESForm::LookupByID<RE::TESObjectCONT>(0x000EA299);
        if (!unownedChest) logger::info("Unowned Chest not found");
        else {
            logger::info("Unowned Chest found {}", unownedChest->GetName());
            unownedChestObjRefr = unownedChest->As<RE::TESObjectREFR>();
            //auto unownedChest3D = unownedChestObjRefr->Get3D();
        }

        unownedCell = RE::TESForm::LookupByID<RE::TESObjectCELL>(0x000EA28B);
        if (!unownedCell) logger::info("Unowned Cell not found");
        else {
            logger::info("Unowned Cell found {}", unownedCell->GetName());
            auto runtimeData = unownedCell->GetRuntimeData();
            for (const auto& ref : unownedCell->GetRuntimeData().references) {
                logger::info("Girdik");
                if (!ref) logger::info("Null object in cell");
                else {
                    logger::info("Object in cell: {}", ref->GetBaseObject()->GetName());
                    logger::info("Object in cell: {}", ref->GetBaseObject()->GetFormEditorID());
                }
			}
        }

    }
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

    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}