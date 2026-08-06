#pragma once

#include "game/effects/EffectConfig.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// Biological/cultural identity used by conquest and similar cross-faction rules.
enum class FactionSpecies_t
{
    Human,
    Progenitor,
    NativeLife,
};

struct FactionIdentityConfig
{
    std::string name;
    std::string descriptiveName;
    std::string noun;
    std::string adjective;
    // When false, the faction cannot sit in / convene the Planetary Council (e.g. aliens).
    // Omitted in JSON defaults to true.
    bool participatesInCouncil = true;
    // Omitted in JSON defaults to Human.
    FactionSpecies_t species = FactionSpecies_t::Human;
};

struct LeaderConfig
{
    std::string name;
    std::string title;
};

struct AITendenciesConfig
{
    bool wealth = false;
    bool power = false;
    bool growth = false;
    bool tech = false;
};

struct FactionFlavorConfig
{
    std::vector<std::string> baseNames;
    std::unordered_map<std::string, std::vector<std::string>> phrases;
};

struct FactionConfig_t
{
    std::string id;
    FactionIdentityConfig identity;
    LeaderConfig leader;
    AITendenciesConfig ai;
    FactionFlavorConfig flavor;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac
