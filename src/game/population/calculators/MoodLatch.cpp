#include "game/population/calculators/MoodLatch.h"

namespace ac
{

void MoodLatch::Forecast_(bool bCondition, bool bHoldPending)
{
    if (m_bActive)
    {
        m_bPending = false;
        return;
    }

    if (bCondition)
    {
        SetPendingAndWarn_();
        return;
    }

    if (!bHoldPending)
    {
        m_bPending = false;
    }
}

MoodLatch::Transition_t MoodLatch::Commit_(bool bCondition)
{
    m_bPending = false;

    if (bCondition)
    {
        if (m_bActive)
        {
            return Transition_t::None;
        }
        m_bActive = true;
        m_rStarted.Emit();
        return Transition_t::Started;
    }

    if (!m_bActive)
    {
        return Transition_t::None;
    }
    m_bActive = false;
    m_rEnded.Emit();
    return Transition_t::Ended;
}

void MoodLatch::Restore_(bool bActive, bool bPending)
{
    m_bActive = bActive;
    m_bPending = bPending;
}

void MoodLatch::SetPendingAndWarn_()
{
    if (m_bActive || m_bPending)
    {
        return;
    }
    m_bPending = true;
    m_rWill.Emit();
}

} // namespace ac
