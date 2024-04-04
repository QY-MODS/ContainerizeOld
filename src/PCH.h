#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <spdlog/sinks/basic_file_sink.h>

using namespace std::literals;
namespace logger = SKSE::log;

namespace RE {

	struct TESSellEvent {
        RE::NiPointer<RE::TESObjectREFR> target;
        RE::NiPointer<RE::TESObjectREFR> seller;
    };
}