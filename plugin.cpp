#include "Manager.h"

// FFI04Sack 000DAB04

Manager* M = nullptr;
bool listen_weight_limit = false;
bool listen_crosshair_ref = true;
bool furniture_entered = false;
bool block_eventsinks = false;
bool block_droptake = false;

RE::NiPointer<RE::TESObjectREFR> furniture = nullptr;

class OurEventSink : public RE::BSTEventSink<RE::TESActivateEvent>,
                     public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                     public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                     public RE::BSTEventSink<RE::TESFurnitureEvent>,
                     public RE::BSTEventSink<RE::TESContainerChangedEvent>{

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

    // Prompts Messagebox
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) {
        
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
        if (!event->actionRef->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        if (!event->objectActivated->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;
        if (event->objectActivated == RE::PlayerCharacter::GetSingleton()->GetGrabbedRef()) return RE::BSEventNotifyControl::kContinue;
        if (!M->listen_activate) return RE::BSEventNotifyControl::kContinue;
        if (!M->IsRealContainer(event->objectActivated.get())) return RE::BSEventNotifyControl::kContinue;
        
        M->ActivateContainer(event->objectActivated.get());
        M->Print();


        return RE::BSEventNotifyControl::kContinue;
    }

    // to disable ref activation and external container-fake container placement
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {

        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;
        if (event->crosshairRef->extraList.GetCount()>1) return RE::BSEventNotifyControl::kContinue;
        if (!listen_crosshair_ref) return RE::BSEventNotifyControl::kContinue;

        if (!M->IsRealContainer(event->crosshairRef.get())) {
            
            // if the fake items are not in it we need to place them (this happens upon load game)
            M->listen_container_change = false;
            listen_crosshair_ref = false; 
            M->HandleFakePlacement(event->crosshairRef.get());
            M->listen_container_change = true;
            listen_crosshair_ref = true;

            return RE::BSEventNotifyControl::kContinue;
        
        }

        if (event->crosshairRef->IsActivationBlocked() && !M->isUninstalled) return RE::BSEventNotifyControl::kContinue;

        
        if (M->isUninstalled) {
            event->crosshairRef->SetActivationBlocked(0);
        } else {
            event->crosshairRef->SetActivationBlocked(1);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // to close chest and save the contents and remove items
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (event->menuName != RE::ContainerMenu::MENU_NAME) return RE::BSEventNotifyControl::kContinue;
        if (!M->listen_menuclose) return RE::BSEventNotifyControl::kContinue;

        if (event->opening) {
            listen_weight_limit = true;
        } else {
            listen_weight_limit = false;
			M->listen_menuclose = false;
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    
    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) {
        
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        if (furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter)
            return RE::BSEventNotifyControl::kContinue;
        if (!furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit)
			return RE::BSEventNotifyControl::kContinue;
        if (event->targetFurniture->GetBaseObject()->formType.underlying() != 40) return RE::BSEventNotifyControl::kContinue;


        auto bench = event->targetFurniture->GetBaseObject()->As<RE::TESFurniture>();
        if (!bench) return RE::BSEventNotifyControl::kContinue;
        auto bench_type = static_cast<std::uint8_t>(bench->workBenchData.benchType.get());
        if (bench_type != 2 && bench_type != 3 && bench_type != 7) return RE::BSEventNotifyControl::kContinue;

        if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
            logger::info("Furniture event: Enter {}", event->targetFurniture->GetName());
            furniture_entered = true;
            furniture = event->targetFurniture;
            M->HandleCraftingEnter();
        }
        else if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
            logger::info("Furniture event: Exit {}", event->targetFurniture->GetName());
            if (event->targetFurniture == furniture) {
                M->HandleCraftingExit();
                furniture_entered = false;
                furniture = nullptr;
            }
        }
        else {
			logger::info("Furniture event: Unknown");
		}


		return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
                                                                   RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        logger::info("ezhuef");
        if (!M->listen_container_change) return RE::BSEventNotifyControl::kContinue;
        logger::info("asdasd");
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!listen_crosshair_ref) return RE::BSEventNotifyControl::kContinue;
        if (furniture_entered) return RE::BSEventNotifyControl::kContinue;
        if (!event->itemCount || event->itemCount > 1) return RE::BSEventNotifyControl::kContinue;
        if (event->oldContainer != 20 && event->newContainer != 20) return RE::BSEventNotifyControl::kContinue;
        logger::info("Item {} went into container {} from container {}.", event->baseObj, event->newContainer, event->oldContainer);

        // to player inventory <-
        if (event->newContainer == 20) {

            if (M->IsRealContainer(event->baseObj) && M->RealContainerHasRegistry(event->baseObj)) {
                auto reference_ = event->reference;
                if (!block_droptake && M->IsARegistry(reference_.native_handle())) {
                    // somehow, including ref=0 bcs that happens sometimes when NPCs give you your dropped items back...
                    logger::info("Item {} went into player inventory from unknown container.", event->baseObj);
                    M->DropTake(event->baseObj, reference_.native_handle());
                    M->Print();
                }
            } else if (M->IsFakeContainer(event->baseObj) && M->ExternalContainerIsRegistered(event->baseObj,event->oldContainer)) {
                logger::info("Unlinking external container.");
                M->UnLinkExternalContainer(event->baseObj, event->oldContainer);
                M->Print();
            }
        }


        // from player inventory ->
        if (event->oldContainer == 20) {
            logger::info("Something left player inventory.");
            // a fake container left player inventory
            if (M->IsFakeContainer(event->baseObj)) {
                logger::info("Fake container left player inventory.");
                // drop event
                if (!event->newContainer) {
                    logger::info("Dropped fake container.");
                    RE::TESObjectREFR* ref = nullptr;
                    if (event->reference.native_handle()) {
						ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(event->reference.native_handle());
					}
                    if (!M->IsFakeContainer(ref)) {
                        // iterate through all objects in the cell................
                        auto player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
                        auto cell_runtime_data = player_cell->GetRuntimeData();
                        for (auto& ref_ : cell_runtime_data.references) {
                            if (M->IsFakeContainer(ref_.get())) {
                                logger::info("Dropped fake container with ref id {}.", ref_->GetFormID());
                                if (!M->SwapDroppedFakeContainer(ref_.get())) {
						            logger::error("Failed to swap fake container.");
                                    return RE::BSEventNotifyControl::kContinue;
					            } else logger::info("Swapped fake container.");
                                break;
                            }
			            }
                    } 
                    else if (!M->SwapDroppedFakeContainer(ref)) {
                        logger::error("Failed to swap fake container.");
                        return RE::BSEventNotifyControl::kContinue;
                    } 
                    else {
                        logger::info("Swapped fake container.");
                        M->Print();
                    }
                }
                // Barter transfer
                else if (RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME) && M->IsCONT(event->newContainer)) {
				    logger::info("Sold container.");
                    block_droptake = true;
                    M->HandleSell(event->baseObj, event->newContainer);
                    block_droptake = false;
                    M->Print();
			    }
                // container transfer
                else if (RE::UI::GetSingleton()->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
                    logger::info("Container transfer.");
            	    // need to register the container: chestrefid -> thiscontainerrefid
                    logger::info("Container menu is open.");
                    M->LinkExternalContainer(event->baseObj, event->newContainer);
                    M->Print();
                }
                else {
                    Utilities::MsgBoxesNotifs::InGame::CustomErrMsg("Unsupported behaviour. Please put back the container you removed from your inventory.");
				    logger::error("Unsupported. Please put back the container you removed from your inventory.");
                }
		    }
            else if (M->IsChest(event->newContainer) && listen_weight_limit) M->InspectItemTransfer();
        }
        

        // check if container has enough capacity

        return RE::BSEventNotifyControl::kContinue;
    }

};


void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        // Start
        auto sources = Settings::LoadINISources();
        if (sources.empty()) {
            logger::critical("Failed to load INI sources.");
            return;
        }
        M = Manager::GetSingleton(sources);
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
        message->type == SKSE::MessagingInterface::kNewGame) {
        if (!M) return;
        // EventSink
        auto* eventSink = OurEventSink::GetSingleton();
        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESContainerChangedEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(eventSink);
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(OurEventSink::GetSingleton());
        SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);
    }
}



#define DISABLE_IF_UNINSTALLED if (!M || M->isUninstalled) return;
void SaveCallback(SKSE::SerializationInterface* serializationInterface) {
    DISABLE_IF_UNINSTALLED 
    M->SendData();
    if (!M->Save(serializationInterface, Settings::kDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
}

void LoadCallback(SKSE::SerializationInterface* serializationInterface) {
    DISABLE_IF_UNINSTALLED
    
    block_eventsinks = true;

    M->Reset();

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

    logger::info("Resetting.");
    listen_weight_limit = false;
    listen_crosshair_ref = true;
    furniture_entered = false;
    logger::info("Receiving Data.");
    M->ReceiveData();
    logger::info("Data loaded from skse co-save.");
    block_eventsinks = false;
}
#undef DISABLE_IF_UNINSTALLED

void InitializeSerialization() {
    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Settings::kDataKey);
    serialization->SetSaveCallback(SaveCallback);
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


