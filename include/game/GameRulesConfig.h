#pragma once

#include <string>

namespace ac
{

// Session game-rule knobs. pauseAtEndOfTurn and autoReturnLowFuelAir are player preferences
// and stay out of save-game serialization.
//
// difficultyId is NOT a preference — it belongs to the campaign, and a save must carry it, or
// loading a Transcend campaign under a user_settings.json that says "citizen" would silently
// reclassify the game. It lives here only until a save system exists; move it to save state
// then, storing the *id* rather than the resolved level so config and mod edits reach existing
// saves. Difficulty is changeable mid-campaign: GameSettings bumps its rules revision and
// FactionEffectsPool samples it, so every faction re-resolves.
struct GameRulesConfig_t
{
    bool pauseAtEndOfTurn = false;
    // When true (default), fuel-using craft auto-path home on auto-select if ending the
    // turn without refuel would destroy them (lethal out-of-fuel damage). Healthy Copters
    // take non-lethal damage and are unaffected.
    bool autoReturnLowFuelAir = true;
    // Id of a level in config/difficulty.json. Empty (the default) defers to that file's
    // "default" key, so the shipping default lives in one place rather than two.
    std::string difficultyId;

    bool operator==(const GameRulesConfig_t&) const = default;
};

} // namespace ac
