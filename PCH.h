#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

using namespace std::literals;


namespace RE {

	struct TESSellEvent {
        RE::NiPointer<RE::TESObjectREFR> target;
        RE::NiPointer<RE::TESObjectREFR> seller;
    };
}