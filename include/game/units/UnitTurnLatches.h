#pragma once

namespace ac
{

// What a unit did this turn and last, rolled at the start of each turn.
class UnitTurnLatches
{
public:
    // A unit that attacked this turn or last may not disengage.
    bool HasAttackedThisTurn() const { return m_bAttackedThisTurn; }
    bool HasAttackedLastTurn() const { return m_bAttackedLastTurn; }
    void MarkAttacked() { m_bAttackedThisTurn = true; }

    // Set by an airdrop until the next turn. Enables post-drop attack modifiers and blocks a
    // second drop while moves remain.
    bool HasAirdroppedThisTurn() const { return m_bAirdroppedThisTurn; }
    void MarkAirdropped() { m_bAirdroppedThisTurn = true; }

    // ThisUnit-scoped Intercept deploy cooldown (mission year when ready again).
    bool IsInterceptReady(int missionYear) const
    {
        return missionYear >= m_interceptReadyMissionYear;
    }
    void DeployIntercept(int readyMissionYear) { m_interceptReadyMissionYear = readyMissionYear; }

    // This turn becomes last turn.
    void Roll()
    {
        m_bAttackedLastTurn = m_bAttackedThisTurn;
        m_bAttackedThisTurn = false;
        m_bAirdroppedThisTurn = false;
    }

private:
    bool m_bAttackedThisTurn = false;
    bool m_bAttackedLastTurn = false;
    bool m_bAirdroppedThisTurn = false;
    // Available when missionYear >= this (0 = always ready at game start).
    int m_interceptReadyMissionYear = 0;
};

} // namespace ac
