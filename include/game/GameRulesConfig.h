#pragma once

namespace ac
{

// Session game-rule knobs (not part of save-game serialization).
struct GameRulesConfig_t
{
    bool pauseAtEndOfTurn = false;
    // When true (default), fuel-using craft auto-path home on auto-select if ending the
    // turn without refuel would destroy them (lethal out-of-fuel damage). Healthy Copters
    // take non-lethal damage and are unaffected.
    bool autoReturnLowFuelAir = true;

    bool operator==(const GameRulesConfig_t&) const = default;
};

} // namespace ac
