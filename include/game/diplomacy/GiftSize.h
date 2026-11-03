#pragma once

namespace ac
{

// How far a gift moves its giver's gift count (terranx-map.md "Gifts").
enum class GiftSize_t
{
    Small,
    // Counts once, and only from 0.
    Notable,
    // Counts once at any count: a base gift short of generous.
    Modest,
    Generous
};

} // namespace ac
