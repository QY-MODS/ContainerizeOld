#include "Manager.h"
// FFI04Sack 000DAB04

Manager* M = nullptr;
bool listen_weight_limit = false;
bool listen_crosshair_ref = true;
bool furniture_entered = false;
bool block_eventsinks = false;
bool block_droptake = false;

// 1) Enables input event check
bool equipped = false;
// 2) set in input event if equip is held
std::string ReShowMenu = "";

FormID fake_id_; // set in equip event
FormID fake_equipped_id; // set in equip event only when equipped and used in container event (consume)
RefID external_container_refid = 0;  // set in input event

RE::NiPointer<RE::TESObjectREFR> furniture = nullptr;

//enum class MenuFlagEx : std::uint32_t {
//    kUnpaused = 1 << 28,
//    kUsesCombatAlertOverlay = 1 << 29,
//    kUsesSlowMotion = 1 << 30
//};


// Thanks and credits to Bloc: https://discord.com/channels/874895328938172446/945560222670393406/1093262407989731338
class ConversationCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {

    std::string rename = "";

    virtual inline void operator()(RE::BSScript::Variable a_result) override {
        if (a_result.IsNoneObject()) {
            logger::trace("Result: None");
        } else if (a_result.IsString()) {
            rename = a_result.GetString();
            logger::trace("Result: {}", rename);
            if (!rename.empty()) {
				M->RenameContainer(rename);
			}
        }
    }

    virtual inline void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&){};
};


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
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
        
        if (!M->IsFakeContainer(event->baseObject)) return RE::BSEventNotifyControl::kContinue;

        fake_equipped_id = event->equipped ? event->baseObject : 0;
        logger::trace("Fake container equipped: {}", fake_equipped_id);

        if (!ReShowMenu.empty()) return RE::BSEventNotifyControl::kContinue;
        
        auto ui_ = RE::UI::GetSingleton();
        if (!(ui_->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui_->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) ||
              ui_->IsMenuOpen(RE::ContainerMenu::MENU_NAME))) return RE::BSEventNotifyControl::kContinue;
        
        if (event->equipped) {
	        logger::trace("Item {} was equipped. equipped: {}", event->baseObject,equipped);
        } else {
            logger::trace("Item {} was unequipped. equipped: {}", event->baseObject, equipped);
        }
        fake_id_ = event->baseObject;
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
        if (!M->getListenActivate()) return RE::BSEventNotifyControl::kContinue;
        if (!M->IsRealContainer(event->objectActivated.get())) return RE::BSEventNotifyControl::kContinue;
        
        logger::trace("Container activated");
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

        // prevent player to catch it in the air
        //if (M->IsFakeContainer(event->crosshairRef.get()->GetBaseObject()->GetFormID())) event->crosshairRef->SetActivationBlocked(1);

        logger::trace("Crosshair ref.");

        if (!M->IsRealContainer(event->crosshairRef.get())) {
            
            // if the fake items are not in it we need to place them (this happens upon load game)
            M->setListenContainerChange(false);
            listen_crosshair_ref = false; 
            M->HandleFakePlacement(event->crosshairRef.get());
            M->setListenContainerChange(true);
            listen_crosshair_ref = true;

            return RE::BSEventNotifyControl::kContinue;
        
        }

        if (event->crosshairRef->IsActivationBlocked() && !M->getUninstalled()) return RE::BSEventNotifyControl::kContinue;

        
        if (M->getUninstalled()) {
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

        //logger::trace("Menu event: {} {}", event->menuName, event->opening ? "opened" : "closed");
        
        if (Utilities::EqStr(event->menuName.c_str(), "CustomMenu") && !event->opening && M->getListenMenuClose()) {
			logger::trace("Rename menu closed.");
            M->setListenMenuClose(false);
            const auto skyrimVM = RE::SkyrimVM::GetSingleton();
            auto vm = skyrimVM ? skyrimVM->impl : nullptr;
            if (!vm) return RE::BSEventNotifyControl::kContinue;
            const char* menuID = "UITextEntryMenu";
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new ConversationCallbackFunctor());
            auto args = RE::MakeFunctionArguments(std::move(menuID));
            vm->DispatchStaticCall("UIExtensions", "GetMenuResultString", args, callback);
            return RE::BSEventNotifyControl::kContinue;
		}


        if (equipped && event->menuName.c_str() == ReShowMenu && !event->opening) {
            logger::trace("menu closed: {}", event->menuName.c_str());
            equipped = false;
            logger::trace("Reverting equip...");
            M->RevertEquip(fake_id_);
            logger::trace("Reverted equip.");
            M->ActivateContainer(fake_id_, true);
            return RE::BSEventNotifyControl::kContinue;
        }


        if (!M->getListenMenuClose()) return RE::BSEventNotifyControl::kContinue;
        if (event->menuName != RE::ContainerMenu::MENU_NAME) return RE::BSEventNotifyControl::kContinue;
        if (event->opening) {
            listen_weight_limit = true;
        } 
        else {
            logger::trace("Our Container menu closed.");
            listen_weight_limit = false;
			M->setListenMenuClose(false);
            logger::trace("listen_menuclose: {}", M->getListenMenuClose());
            if (!ReShowMenu.empty()){
                M->UnHideReal(fake_id_);
                if (ReShowMenu == RE::ContainerMenu::MENU_NAME && !external_container_refid) {
                    logger::warn("External container refid is 0.");
                }
                if (ReShowMenu != RE::ContainerMenu::MENU_NAME && external_container_refid) {
                    logger::warn("ReShowMenu is not ContainerMenu.");
                }
                if (external_container_refid && ReShowMenu == RE::ContainerMenu::MENU_NAME) {
                    M->RevertEquip(fake_id_, external_container_refid);
                }
                if (M->_other_settings[Settings::otherstuffKeys[2]]) {
                    logger::trace("Returning to initial menu: {}", ReShowMenu);
                    if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
                        if (external_container_refid && ReShowMenu == RE::ContainerMenu::MENU_NAME) {
                            if (auto a_objref = RE::TESForm::LookupByID<RE::TESObjectREFR>(external_container_refid)) {
                                auto player_ref = RE::PlayerCharacter::GetSingleton();
                                if (auto has_container = a_objref->HasContainer()) {
                                    logger::trace("HasContainer: {}", has_container);
                                    if (auto container = a_objref->As<RE::TESObjectCONT>()) {
                                        OpenContainer(a_objref, 0);
                                        //container->Activate(a_objref, player_ref, 0, container, 1);
                                    } 
                                    else if (auto container_ = a_objref->GetBaseObject()->As<RE::TESObjectCONT>()) {
                                        OpenContainer(a_objref, 0);
                                        //container_->Activate(a_objref, player_ref, 0, container_, 1);
                                    } 
                                    else {
                                        logger::trace("has container but could not activate.");
                                        OpenContainer(a_objref, 3);
                                    }
                                } else a_objref->ActivateRef(player_ref, 0, a_objref->GetBaseObject(), 1, 0);
                            }
                        } 
                        else queue->AddMessage(ReShowMenu, RE::UI_MESSAGE_TYPE::kShow, nullptr);
                    }
                    else logger::warn("Failed to return to initial menu.");
                }
                external_container_refid = 0;
                ReShowMenu = "";
            }
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

        logger::trace("Furniture event");

        auto bench = event->targetFurniture->GetBaseObject()->As<RE::TESFurniture>();
        if (!bench) return RE::BSEventNotifyControl::kContinue;
        auto bench_type = static_cast<std::uint8_t>(bench->workBenchData.benchType.get());
        if (bench_type != 2 && bench_type != 3 && bench_type != 7) return RE::BSEventNotifyControl::kContinue;

        if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
            logger::trace("Furniture event: Enter {}", event->targetFurniture->GetName());
            furniture_entered = true;
            furniture = event->targetFurniture;
            M->HandleCraftingEnter();
        }
        else if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
            logger::trace("Furniture event: Exit {}", event->targetFurniture->GetName());
            if (event->targetFurniture == furniture) {
                M->HandleCraftingExit();
                furniture_entered = false;
                furniture = nullptr;
            }
        }
        else {
			logger::trace("Furniture event: Unknown");
		}


		return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
                                                                   RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!M->getListenContainerChange()) return RE::BSEventNotifyControl::kContinue;
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (!listen_crosshair_ref) return RE::BSEventNotifyControl::kContinue;
        if (furniture_entered) return RE::BSEventNotifyControl::kContinue;
        if (!event->itemCount) return RE::BSEventNotifyControl::kContinue;
        if (event->oldContainer != 20 && event->newContainer != 20) return RE::BSEventNotifyControl::kContinue;


        logger::trace("Container changed event.");

        // to player inventory <-
        if (event->newContainer == 20) {
            if (event->itemCount == 1 && M->IsRealContainer(event->baseObj) &&
                M->RealContainerHasRegistry(event->baseObj)) {
                auto reference_ = event->reference;
                if (!block_droptake && M->IsARegistry(reference_.native_handle())) {
                    // somehow, including ref=0 bcs that happens sometimes when NPCs give you your dropped items back...
                    logger::info("Item {} went into player inventory from unknown container.", event->baseObj);
                    logger::trace("Dropped item native_handle: {}", reference_.native_handle());
                    M->DropTake(event->baseObj, reference_.native_handle());
                    M->Print();
                }
            } else if (M->IsFakeContainer(event->baseObj) && M->ExternalContainerIsRegistered(event->baseObj,event->oldContainer)) {
                logger::trace("Unlinking external container.");
                M->UnLinkExternalContainer(event->baseObj, event->oldContainer);
                M->Print();
            }
        }


        // from player inventory ->
        if (event->oldContainer == 20) {
            // a fake container left player inventory
            if (M->IsFakeContainer(event->baseObj)) {
                logger::trace("Fake container left player inventory.");
                // drop event
                if (!event->newContainer) {
                    auto swapped = false;
                    logger::trace("Dropped fake container.");
                    RE::TESObjectREFR* ref =
                        RE::TESForm::LookupByID<RE::TESObjectREFR>(event->reference.native_handle());
                    if (ref) logger::trace("Dropped ref name: {}", ref->GetBaseObject()->GetName());
                    if (!ref || ref->GetBaseObject()->GetFormID() != event->baseObj) {
                        // iterate through all objects in the cell................
                        logger::info("Iterating through all references in the cell.");
                        auto player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
                        auto& cell_runtime_data = player_cell->GetRuntimeData();
                        for (auto& ref_ : cell_runtime_data.references) {
                            if (!ref_) continue;
                            if (ref_.get()->GetBaseObject()->GetFormID()==event->baseObj) {
                                logger::trace("Dropped fake container with ref id {}.", ref_->GetFormID());
                                if (!M->SwapDroppedFakeContainer(ref_.get())) {
						            logger::error("Failed to swap fake container.");
                                    return RE::BSEventNotifyControl::kContinue;
                                } 
                                else {
                                    logger::trace("Swapped fake container.");
                                    swapped = true;
                                }
                                break;
                            }
			            }
                    } 
                    else if (!M->SwapDroppedFakeContainer(ref)) {
                        logger::error("Failed to swap fake container.");
                        return RE::BSEventNotifyControl::kContinue;
                    } 
                    else {
                        logger::trace("Swapped fake container.");
                        swapped = true;
                        M->Print();
                    }
                    // consumed
                    if (!swapped && event->baseObj==fake_equipped_id) {
                        logger::trace("new container: {}", event->newContainer);
                        logger::trace("old container: {}", event->oldContainer);
                        logger::trace("Sending to handle consume.");
                        fake_equipped_id = 0;
                        equipped = false;
                        M->HandleConsume(event->baseObj);
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
                    logger::trace("Container transfer.");
            	    // need to register the container: chestrefid -> thiscontainerrefid
                    logger::trace("Container menu is open.");
                    M->LinkExternalContainer(event->baseObj, event->newContainer);
                    M->Print();
                }
                else {
                    Utilities::MsgBoxesNotifs::InGame::CustomErrMsg("Unsupported behaviour. Please put back the container you removed from your inventory.");
				    logger::error("Unsupported. Please put back the container you removed from your inventory.");
                }
		    }
            // check if container has enough capacity
            else if (M->IsChest(event->newContainer) && listen_weight_limit) M->InspectItemTransfer(event->newContainer);
        }



        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* evns, RE::BSTEventSource<RE::InputEvent*>*) {
        if (block_eventsinks) return RE::BSEventNotifyControl::kContinue;
        if (!*evns) return RE::BSEventNotifyControl::kContinue;
        if (!equipped) return RE::BSEventNotifyControl::kContinue; // this also ensures that we have only the menus we want

        for (RE::InputEvent* e = *evns; e; e = e->next) {
            if (e->eventType.get() == RE::INPUT_EVENT_TYPE::kButton) {
                RE::ButtonEvent* a_event = e->AsButtonEvent();
                RE::IDEvent* id_event = e->AsIDEvent();
                auto& userEvent = id_event->userEvent;

                if (a_event->IsPressed()) {
                    if (a_event->IsRepeating() && a_event->HeldDuration() > .3f && a_event->HeldDuration() < .32f){
                            logger::trace("User event: {}", userEvent.c_str());
                            // we accept : "accept","RightEquip", "LeftEquip", "equip", "toggleFavorite"
                            auto userevents = RE::UserEvents::GetSingleton();
                            if (!(userEvent == userevents->accept || userEvent == userevents->rightEquip ||
                                  userEvent == userevents->leftEquip || userEvent == userevents->equip)) {
                                logger::trace("User event not accepted.");
                                continue;
                            }
                            if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
                                auto ui = RE::UI::GetSingleton();
                                RefID refHandle = 0;
                                if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
                                    ReShowMenu = RE::InventoryMenu::MENU_NAME;
                                    queue->AddMessage(RE::TweenMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                                } 
                                else if (ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) ReShowMenu = RE::FavoritesMenu::MENU_NAME;
                                else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
                                    ReShowMenu = RE::ContainerMenu::MENU_NAME;
                                    const auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
                                    refHandle = menu ? menu->GetTargetRefHandle() : 0;
                                    RE::TESObjectREFRPtr ref;
                                    RE::LookupReferenceByHandle(refHandle, ref);
                                    if (ref) external_container_refid = ref->GetFormID();
                                    else logger::warn("Failed to get ref from handle.");
                                    logger::trace("External container refid: {}", external_container_refid);
                                }
                                else continue;
                                queue->AddMessage(ReShowMenu, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                                return RE::BSEventNotifyControl::kContinue;
                            }
                    }
                } else if (a_event->IsUp()) equipped = false;
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
        bool is_before_0_7 = false;
        
        auto temp = Utilities::DecodeTypeCode(type);

        if (version == Settings::kSerializationVersion-1) {
            logger::warn("Loading data is from an older version < v0.7. Recieved ({}) - Expected ({}) for Data Key ({})",
							 version, Settings::kSerializationVersion, temp);
			is_before_0_7= true;
		}
        else if (version != Settings::kSerializationVersion) {
            logger::critical("Loaded data has incorrect version. Recieved ({}) - Expected ({}) for Data Key ({})",
                             version, Settings::kSerializationVersion, temp);
            continue;
        }
        switch (type) {
            case Settings::kDataKey: {
                logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
                if (!M->Load(serializationInterface, is_before_0_7)) {
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

void SetupLog() {

    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));

#ifndef NDEBUG
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
#endif

    logger::info("Name of the plugin is {}.", pluginName);
    logger::info("Version of the plugin is {}", Utilities::GetPluginVersion(4));

}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    SKSE::Init(skse);
    InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}


