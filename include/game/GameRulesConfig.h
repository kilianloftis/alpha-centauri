#pragma once

#include <string>

namespace ac
{

// Session game rules. pauseAtEndOfTurn and autoReturnLowFuelAir are player preferences.
// TODO: difficultyId and nativeLifeLevelId are campaign state; save them as ids.
struct GameRulesConfig_t
{
    bool pauseAtEndOfTurn = false;
    // Fuel-using craft auto-path home when ending the turn unrefuelled would destroy them.
    bool autoReturnLowFuelAir = true;
    // When true, skip global / wreckage pod scatter; only start-ring pods are placed.
    bool noUnityScattering = false;
    // A level in config/difficulty.json; empty means its "default".
    std::string difficultyId;
    // A level in config/native_life.json; empty means its "default".
    std::string nativeLifeLevelId;

    bool operator==(const GameRulesConfig_t&) const = default;
};

} // namespace ac
