#pragma once

#include "lib/Signal.h"

namespace ac
{

// The pending -> active latch shared by RiotCalculator and GoldenAgeCalculator.
//
// Both mood conditions run the same two-phase lifecycle across a turn:
//   Forecast (Population stage) — condition holds, so warn the player and set pending. No
//     gameplay effect yet; the player still has PlayerActions to respond.
//   Commit (Mood stage, after PlayerActions) — re-evaluate and latch active, or release.
//
// Only the bookkeeping around it differs (a riot ages a forced timer and counts consecutive
// turns), so the derived calculators supply the condition and react to the transition rather
// than restating the state machine. Not a polymorphic base: no virtuals, protected
// non-virtual destructor, never held by base pointer.
class MoodLatch
{
public:
    // True once Commit has latched: gameplay effects apply.
    bool IsActive() const { return m_bActive; }
    // True after Forecast when the condition would latch but Commit has not run yet.
    bool IsPending() const { return m_bPending; }

protected:
    MoodLatch(Signal<>& rWill, Signal<>& rStarted, Signal<>& rEnded)
        : m_rWill(rWill)
        , m_rStarted(rStarted)
        , m_rEnded(rEnded)
    {
    }
    ~MoodLatch() = default;

    enum class Transition_t
    {
        None,
        Started,
        Ended,
    };

    // Set pending and emit the warning signal when the condition newly holds. An already
    // active latch clears stale pending and waits for Commit to decide. bHoldPending keeps a
    // pending flag alive through a false condition, for a source the condition cannot express
    // (a forced riot does not change composition).
    void Forecast_(bool bCondition, bool bHoldPending);

    // Latch or release, emitting started / ended on the edge. Returns which edge was crossed
    // so the caller can do its own bookkeeping without re-deriving it.
    Transition_t Commit_(bool bCondition);

    // Save/load: set both flags without emitting.
    void Restore_(bool bActive, bool bPending);

    // Warn without the Forecast condition, for a source that sets pending directly.
    void SetPendingAndWarn_();

private:
    Signal<>& m_rWill;
    Signal<>& m_rStarted;
    Signal<>& m_rEnded;
    bool m_bActive = false;
    bool m_bPending = false;
};

} // namespace ac
