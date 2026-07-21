#pragma once

#include "game/faction/base/BaseTypes.h"
#include <algorithm>
#include <compare>
#include <stdexcept>

namespace ac
{

// Symmetric unordered pair of distinct factions (canonical min/max order).
struct FactionPair
{
    FactionId_t a = 0;
    FactionId_t b = 0;

    static FactionPair Canonical(FactionId_t x, FactionId_t y)
    {
        if (x == y)
        {
            throw std::invalid_argument("FactionPair: cannot pair a faction with itself");
        }
        return {std::min(x, y), std::max(x, y)};
    }

    friend bool operator==(const FactionPair&, const FactionPair&) = default;
    friend auto operator<=>(const FactionPair&, const FactionPair&) = default;
};

// Directed A → B pair (does not canonicalize). Self-pairs are rejected.
struct DirectedFactionPair
{
    FactionId_t from = 0;
    FactionId_t to = 0;

    static DirectedFactionPair Make(FactionId_t from, FactionId_t to)
    {
        if (from == to)
        {
            throw std::invalid_argument("DirectedFactionPair: cannot pair a faction with itself");
        }
        return {from, to};
    }

    friend bool operator==(const DirectedFactionPair&, const DirectedFactionPair&) = default;
    friend auto operator<=>(const DirectedFactionPair&, const DirectedFactionPair&) = default;
};

} // namespace ac
