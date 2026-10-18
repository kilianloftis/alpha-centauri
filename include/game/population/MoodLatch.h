#pragma once

#include "lib/Signal.h"

namespace ac
{

// The pending -> active latch RiotLatch and GoldenAgeLatch share: Forecast warns in the
// Population stage, so the player can respond before Commit latches in the Mood stage.
class MoodLatch
{
public:
    // True once Commit has latched: gameplay effects apply.
    bool IsActive() const { return m_bActive; }
    // True after Forecast when the condition would latch but Commit has not run yet.
    bool IsPending() const { return m_bPending; }

    // Set pending and emit the warning signal when the condition newly holds. An already
    // active latch clears stale pending and waits for Commit to decide.
    void Forecast(bool bCondition);

protected:
    MoodLatch(Signal<>& rWill, Signal<>& rStarted, Signal<>& rEnded)
        : m_rWill(rWill)
        , m_rStarted(rStarted)
        , m_rEnded(rEnded)
    {
    }
    // Not a polymorphic base: never held or deleted through a MoodLatch pointer.
    ~MoodLatch() = default;

    enum class Transition_t
    {
        None,
        Started,
        Ended,
    };

    // Latch or release, emitting started / ended on the edge. Returns which edge was crossed
    // so the caller can do its own bookkeeping without re-deriving it.
    Transition_t Commit(bool bCondition);

    // Save/load: set both flags without emitting.
    void RestoreState(bool bActive, bool bPending);

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
