#include "Manager.h"

// FFI04Sack 000DAB04

Manager* M = nullptr;
bool listen_weight_limit = false;
bool listen_crosshair_ref = true;
bool furniture_entered = false;
bool block_eventsinks = false;
bool block_droptake = false;

bool tus_basili = false;
bool equipped = false;
bool showMenu = false;
FormID fake_id;

RE::NiPointer<RE::TESObjectREFR> furniture = nullptr;


//enum class MenuFlagEx : std::uint32_t {
//    kUnpaused = 1 << 28,
//    kUsesCombatAlertOverlay = 1 << 29,
//    kUsesSlowMotion = 1 << 30
//};


class OurEventSink : public RE::BSTEventSink<RE::TESEquipEvent>,
                     public RE::BSTEventSink<RE::TESActivateEvent>,
                     public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                     public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                     public RE::BSTEventSink<RE::TESFurnitureEvent>,
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


    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) {
        logger::info("Equip event.");
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        auto ui_ = RE::UI::GetSingleton();
        if (!ui_->IsMenuOpen(RE::InventoryMenu::MENU_NAME) && !ui_->IsMenuOpen(RE::FavoritesMenu::MENU_NAME))
            return RE::BSEventNotifyControl::kContinue;
        if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        if (showMenu) return RE::BSEventNotifyControl::kContinue;
        if (!M->IsFakeContainer(event->baseObject)) return RE::BSEventNotifyControl::kContinue;
        if (event->equipped) {
	        logger::info("Item {} was equipped. equipped: {}", event->baseObject,equipped);
        } else {
            logger::info("Item {} was unequipped. equipped: {}", event->baseObject, equipped);
        }
        fake_id = event->baseObject;
        equipped = true;
        return RE::BSEventNotifyControl::kContinue;
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

        // for things like horses or NPCs
   //     if (event->menuName == RE::ContainerMenu::MENU_NAME && event->opening){
   //         auto container_owner = Utilities::GetMenuOwner<RE::ContainerMenu>();
   //         if (container_owner) {
   //             logger::info("Container owner: {}", container_owner->GetName());
   //             logger::info("Container owner refid: {}", container_owner->GetFormID());
   //         } else {
			//	logger::info("Container owner: unknown");
			//}
   //         M->listen_container_change = false;
   //         listen_crosshair_ref = false;
   //         M->HandleFakePlacement(container_owner);
   //         M->listen_container_change = true;
   //         listen_crosshair_ref = true;
   //     }
  //      if (event->menuName == RE::ContainerMenu::MENU_NAME && event->opening) {
  //          // https:// github.com/Exit-9B/Dont-Eat-Spell-Tomes/blob/7b7f97353cc6e7ccfad813661f39710b46d82972/src/SpellTomeManager.cpp#L23-L32
  //          RE::TESObjectREFR* container = nullptr;
  //          const auto ui = RE::UI::GetSingleton();
  //          const auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
  //          const auto movie = menu ? menu->uiMovie : nullptr;

  //          if (movie) {
  //              // "Menu_mc" is stored in the class, but it's different in VR and we haven't RE'd it yet
  //              RE::GFxValue isViewingContainer;
  //              movie->Invoke("Menu_mc.isViewingContainer", &isViewingContainer, nullptr, 0);

  //              if (isViewingContainer.GetBool()) {
  //                  auto refHandle = menu->GetTargetRefHandle();
  //                  RE::TESObjectREFRPtr refr;
  //                  RE::LookupReferenceByHandle(refHandle, refr);

  //                  container = refr.get();
  //                  if (M->IsChest(container->GetFormID())) {
		//				logger::info("Chest opened.");
  //                      menu->menuFlags.reset(static_cast<RE::IMenu::Flag>(MenuFlagEx::kUnpaused));

		//			}
  //              }
  //          }
		//}

        if ((event->menuName == RE::InventoryMenu::MENU_NAME || event->menuName == RE::FavoritesMenu::MENU_NAME) &&
            !event->opening && showMenu) {
            logger::info("Inventory menu closed.");
            equipped = false;
            logger::info("Reverting equip...");
            M->RevertEquip(fake_id);
            logger::info("Reverted equip.");
            M->ActivateContainer(fake_id,true);

            //auto msgQ = RE::UIMessageQueue::GetSingleton();
            //msgQ->AddMessage(RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            //if (auto container_menu = RE::UI::GetSingleton()->GetMenu(RE::ContainerMenu::MENU_NAME)) {
            //    container_menu->depthPriority = 3;
            //};
            //if (!RE::UI::GetSingleton()->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            //    //msgQ->AddMessage(RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            //    container_menu->uiMovie->SetVisible(true);
            //}
                

        }
        else if (event->menuName == RE::ContainerMenu::MENU_NAME && !event->opening && showMenu) {
            logger::info("Container menu closed.");
            showMenu = false;
            M->UnHideReal(fake_id);
            /*const auto ui = RE::UI::GetSingleton();
            const auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
            menu->menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame);*/
        }



        if (!M->listen_menuclose) return RE::BSEventNotifyControl::kContinue;
        if (event->menuName != RE::ContainerMenu::MENU_NAME) return RE::BSEventNotifyControl::kContinue;

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
        if (!M->listen_container_change) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!listen_crosshair_ref) return RE::BSEventNotifyControl::kContinue;
        if (furniture_entered) return RE::BSEventNotifyControl::kContinue;
        if (!event->itemCount || event->itemCount > 1) return RE::BSEventNotifyControl::kContinue;
        if (event->oldContainer != 20 && event->newContainer != 20) return RE::BSEventNotifyControl::kContinue;

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
                    RE::TESObjectREFR* ref =
                        RE::TESForm::LookupByID<RE::TESObjectREFR>(event->reference.native_handle());
                    if (ref) logger::info("Dropped ref name: {}", ref->GetBaseObject()->GetName());
                    if (!M->IsFakeContainer(ref)) {
                        // iterate through all objects in the cell................
                        logger::info("Iterating through all references in the cell.");
                        auto player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
                        auto cell_runtime_data = player_cell->GetRuntimeData();
                        for (auto& ref_ : cell_runtime_data.references) {
                            if (M->IsFakeContainer(ref_.get()) && ref_.get()->GetBaseObject()->GetFormID()==event->baseObj) {
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
            // check if container has enough capacity
            else if (M->IsChest(event->newContainer) && listen_weight_limit) M->InspectItemTransfer();
        }

        // consumed
        /*if (M->IsFakeContainer(event->baseObj) && !event->newContainer){
            logger::info("new container: {}", event->newContainer);
            logger::info("old container: {}", event->oldContainer);
            logger::info("Sending to handle consume.");
            M->HandleConsume(event->baseObj);
		}*/


        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* evns, RE::BSTEventSource<RE::InputEvent*>*) {
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!*evns) return RE::BSEventNotifyControl::kContinue;
        if (!equipped) return RE::BSEventNotifyControl::kContinue;
        if (showMenu) return RE::BSEventNotifyControl::kContinue;

        for (RE::InputEvent* e = *evns; e; e = e->next) {
            if (e->eventType.get() == RE::INPUT_EVENT_TYPE::kButton) {
                RE::ButtonEvent* a_event = e->AsButtonEvent();
                RE::IDEvent* id_event = e->AsIDEvent();
                auto userEvent = id_event->userEvent;

                if (a_event->IsPressed()) {
                    if (a_event->IsRepeating() && a_event->HeldDuration() > .3f && a_event->HeldDuration() < .32f){
                            logger::info("User event: {}", userEvent.c_str());
                            // we accept : "accept","RightEquip", "LeftEquip", "equip", "toggleFavorite"
                            auto userevents = RE::UserEvents::GetSingleton();
                            if (!(userEvent == userevents->accept || userEvent == userevents->rightEquip ||
                                  userEvent == userevents->leftEquip || userEvent == userevents->equip)) {
                                logger::info("User event not accepted.");
                                continue;
                            }
                        if (RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
                            if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
                                showMenu = true;
                                queue->AddMessage(RE::TweenMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                                queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                                return RE::BSEventNotifyControl::kContinue;
                            }
					        
                        }
                        else if (RE::UI::GetSingleton()->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) {
                            if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
                                showMenu = true;
                                queue->AddMessage(RE::FavoritesMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,nullptr);
                                return RE::BSEventNotifyControl::kContinue;
                            }
                        }
                    }
                } else if (a_event->IsUp() && equipped) equipped = false;
            }
        }
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
        eventSourceHolder->AddEventSink<RE::TESEquipEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESContainerChangedEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(eventSink);
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(OurEventSink::GetSingleton());
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(eventSink);
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
    
    logger::info("Loading Data from skse co-save.");
    
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


