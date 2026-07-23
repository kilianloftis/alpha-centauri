#pragma once

namespace ac
{

// Session game-rule knobs (not part of save-game serialization).
struct GameRulesConfig_t
{
    bool pauseAtEndOfTurn = false;
    // When set, every faction treats the whole map as explored (no shroud).
    bool removeShroud = false;
};

} // namespace ac
