#pragma once

#include "game/units/AirdropRules.h"

#include <string>

namespace ac
{

// Player-facing deny text for NoticePopup when TryAirdrop / targeting fails.
inline std::string AirdropFailReasonMessage(AirdropFailReason_t reason)
{
    switch (reason)
    {
        case AirdropFailReason_t::None:
            return {};
        case AirdropFailReason_t::NotCapable:
            return "This unit cannot make an airdrop.";
        case AirdropFailReason_t::NotOnLaunchPad:
            return "Airdrops must be launched from a friendly base or airbase.";
        case AirdropFailReason_t::NoMovesRemaining:
            return "Airdrops require full movement remaining.";
        case AirdropFailReason_t::AlreadyAirdropped:
            return "This unit has already airdropped this turn.";
        case AirdropFailReason_t::OutOfRange:
            return "That destination is beyond airdrop range.";
        case AirdropFailReason_t::CannotEnter:
            return "This unit cannot enter that tile.";
        case AirdropFailReason_t::EnemyOccupied:
            return "Cannot airdrop onto a tile occupied by enemy or neutral units.";
        case AirdropFailReason_t::Interdicted:
            return "Air interdiction prevents an airdrop at that location.";
    }
    return "Airdrop failed.";
}

} // namespace ac
