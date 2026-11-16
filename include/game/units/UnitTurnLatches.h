#pragma once

#include <cstdint>

namespace ac
{

// Facts recorded about a unit this turn / last turn. Heal and disengage each interpret the
// ledger (docs/thinker/terranx-map.md "Unit healing"; DisengageRules for Attacked).
enum class UnitTurnEvent_t : unsigned
{
    MoveSpend = 0,
    Attacked,   // initiated an attack
    Combat,     // fought as either side / bombard halt
    TookDamage,
    Airdropped,
    Count
};

// What a unit did this turn and last, rolled at the start of each turn.
class UnitTurnLatches
{
public:
    void Record(UnitTurnEvent_t event);
    bool HasThisTurn(UnitTurnEvent_t event) const;
    bool HasLastTurn(UnitTurnEvent_t event) const;

    // Convenience for airdrop rules / HasAirdroppedThisTurn condition.
    bool HasAirdroppedThisTurn() const { return HasThisTurn(UnitTurnEvent_t::Airdropped); }

    // ThisUnit-scoped Intercept deploy cooldown (mission year when ready again).
    bool IsInterceptReady(int missionYear) const
    {
        return missionYear >= m_interceptReadyMissionYear;
    }
    void DeployIntercept(int readyMissionYear) { m_interceptReadyMissionYear = readyMissionYear; }

    // This turn's events become last turn's; this turn clears.
    void Advance();

private:
    using EventSet_t = std::uint8_t;

    static EventSet_t Bit_(UnitTurnEvent_t event);
    static bool Contains_(EventSet_t set, UnitTurnEvent_t event);

    EventSet_t m_thisTurn = 0;
    EventSet_t m_lastTurn = 0;
    // Available when missionYear >= this (0 = always ready at game start).
    int m_interceptReadyMissionYear = 0;
};

} // namespace ac
