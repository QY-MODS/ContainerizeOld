#include "Manager.h"

// FFI04Sack 000DAB04

//Manager* M;

//
//class OurEventSink : public RE::BSTEventSink<RE::TESActivateEvent>,
//                     public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
//                     public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
//                     public RE::BSTEventSink<RE::InputEvent*> {
//    OurEventSink() = default;
//    OurEventSink(const OurEventSink&) = delete;
//    OurEventSink(OurEventSink&&) = delete;
//    OurEventSink& operator=(const OurEventSink&) = delete;
//    OurEventSink& operator=(OurEventSink&&) = delete;
//
//public:
//    static OurEventSink* GetSingleton() {
//        static OurEventSink singleton;
//        return &singleton;
//    }
//    // 2) to put saved items to chest and to open chest (DONE)
//    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
//                                          RE::BSTEventSource<RE::TESActivateEvent>*) {
//        //logger::info("asda");
//        if (!event) return RE::BSEventNotifyControl::kContinue;
//        if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
//        if (!event->actionRef->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
//        if (!event->objectActivated->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;
//        /*if (!M->listen_activate) return RE::BSEventNotifyControl::kContinue;
//        if (!M->RefIsContainer(event->objectActivated.get())) return RE::BSEventNotifyControl::kContinue;*/
//        
//        //logger::info("container has {} extra data", extradatalist.size());
//  //      for (auto it = extradatalist.begin(); it != extradatalist.end(); ++it) {
//		//	it->GetType();
//  // //         if (extra->GetType() == RE::ExtraDataType::kContainerChanges) {
//		//	//	auto containerchanges = static_cast<RE::ExtraContainerChanges*>(extra);
//		//	//	auto changes = containerchanges->changes;
//  // //             for (auto it2 = changes.begin(); it2 != changes.end(); ++it2) {
//		//	//		auto change = *it2;
//		//	//		auto item = change->item;
//		//	//		auto count = change->countDelta;
//		//	//		logger::info("Item {} count {}", item->GetName(), count);
//		//	//		M->AddItemToContainer(item, count);
//		//	//	}
//		//	//}
//		//}
//        
//        
//        //M->ActivateContainer(event->objectActivated.get());
//
//        logger::info("asdaasd");
//
//        return RE::BSEventNotifyControl::kContinue;
//    }
//
//    // 1) to disable ref activation (DONE)
//    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
//                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
//
//        //logger::info("asd2");
//        if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
//        if (event->crosshairRef->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;
//        if (event->crosshairRef->extraList.GetCount()>1) return RE::BSEventNotifyControl::kContinue;
//
//        //logger::info("Crosshair is over {}", event->crosshairRef->GetBaseObject()->GetName());
//        
//  //      if (M->RefIsContainer(event->crosshairRef.get())) {
//		//	event->crosshairRef->SetActivationBlocked(1);
//  //          logger::info("Ref activation disabled");
//		//}
//
//        logger::info("asd3");
//        return RE::BSEventNotifyControl::kContinue;
//    }
//    
//    // to close close chest and save the contents and remove items (MAYBE)
//    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
//                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
//        //logger::info("lm12");
//        if (!event) return RE::BSEventNotifyControl::kContinue;
//        //if (!M->listen_menuclose) return RE::BSEventNotifyControl::kContinue;
//
//  //      if (event->menuName == "ContainerMenu" && !event->opening) {
//		//	logger::info("Container menu closed");
//		//	M->listen_menuclose = false;
//  //          M->DeactivateContainer();
//  //          logger::info("lm13");
//		//}
//
//        return RE::BSEventNotifyControl::kContinue;
//    }
//
//
//    // (MAYBE)
//    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* eventPtr, RE::BSTEventSource<RE::InputEvent*>*) {
//        if (!eventPtr) return RE::BSEventNotifyControl::kContinue;
//
//        auto* event = *eventPtr;
//        if (!event) return RE::BSEventNotifyControl::kContinue;
//
//        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
//            auto* buttonEvent = event->AsButtonEvent();
//            auto dxScanCode = buttonEvent->GetIDCode();
//            logger::info("Pressed key {}", dxScanCode);
//        }
//
//        return RE::BSEventNotifyControl::kContinue;
//    }
//};
//
//
//void OnMessage(SKSE::MessagingInterface::Message* message) {
//    /*if (message->type == SKSE::MessagingInterface::kInputLoaded)
//        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(OurEventSink::GetSingleton());*/
//    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
//        // Start
//        //auto sources = Settings::LoadINISettings();
//        //M = Manager::GetSingleton(sources);
//        
//        // EventSink
//        auto* eventSink = OurEventSink::GetSingleton();
//        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
//
//        eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
//        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(OurEventSink::GetSingleton());
//        SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);
//    }
//}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    SKSE::Init(skse);
    //SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}