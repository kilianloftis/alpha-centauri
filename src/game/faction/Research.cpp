#include "game/faction/Research.h"

namespace ac
{

Research::Research()
{
}

int Research::GetResearchPoints() const
{
    return m_researchPoints;
}

void Research::AddResearchPoints(int points)
{
    m_researchPoints += points;
    if (m_researchPoints >= m_pointsNeededForNextTech)
    {
        m_researchPoints -= m_pointsNeededForNextTech;
    }
}

void Research::SetPointsNeededForNextTech(int points)
{
    m_pointsNeededForNextTech = points;
    while (m_researchPoints >= m_pointsNeededForNextTech)
    {
        m_researchPoints -= m_pointsNeededForNextTech;
    }
}

int Research::GetPointsNeededForNextTech() const
{
    return m_pointsNeededForNextTech;
}

Research::~Research()
{
}

} // namespace ac
