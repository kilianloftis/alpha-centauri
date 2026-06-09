#include "game/faction/population/RiotCalculator.h"

namespace ac
{

void RiotCalculator::NotifyPopGrown(bool bDroneRiotCondition)
{
    if (!m_bRioting && bDroneRiotCondition)
    {
        will_riot.emit();
    }
}

void RiotCalculator::Update(bool bDroneRiotCondition)
{
    if (bDroneRiotCondition)
    {
        m_bRioting = true;
        is_rioting.emit();
    }
    else if (m_bRioting)
    {
        m_bRioting = false;
        riot_ended.emit();
    }
}

bool RiotCalculator::IsRioting() const
{
    return m_bRioting;
}

} // namespace ac
