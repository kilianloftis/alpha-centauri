#pragma once

namespace ac
{

// Session game-rule knobs (not part of save-game serialization).
struct GameRulesConfig_t
{
    bool pauseAtEndOfTurn = false;

    bool operator==(const GameRulesConfig_t&) const = default;
};

} // namespace ac
