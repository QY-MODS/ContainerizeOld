#include "Manager.h"


SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    SKSE::Init(skse);
    return true;
}