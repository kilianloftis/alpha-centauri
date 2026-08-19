#pragma once

namespace ac
{

// Session difficulty. The integer value is the bureaucracy formula input
// (Citizen = 0 … Transcend = 5).
enum class Difficulty_t
{
    Citizen = 0,
    Specialist = 1,
    Talent = 2,
    Librarian = 3,
    Thinker = 4,
    Transcend = 5,
};

// Session game-rule knobs (not part of save-game serialization).
struct GameRulesConfig_t
{
    bool pauseAtEndOfTurn = false;
    // When true (default), fuel-using craft auto-path home on auto-select if ending the
    // turn without refuel would destroy them (lethal out-of-fuel damage). Healthy Copters
    // take non-lethal damage and are unaffected.
    bool autoReturnLowFuelAir = true;
    Difficulty_t difficulty = Difficulty_t::Citizen;

    bool operator==(const GameRulesConfig_t&) const = default;
};

} // namespace ac
