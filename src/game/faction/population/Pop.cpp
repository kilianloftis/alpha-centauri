#include "game/faction/population/Pop.h"
#include "game/faction/population/PopTypeConfigParser.h"
#include <cmath>

namespace ac
{

Pop::Pop(const PopTypeConfig& rConfig)
    : m_pConfig(&rConfig)
    , m_tileId(-1)
{
}

Pop::~Pop()
{
}

const char* Pop::GetPopType() const
{
    return m_pConfig->id.c_str();
}

bool Pop::IsWorker() const
{
    return m_pConfig->bCanWorkTile;
}

bool Pop::IsDrone() const
{
    return m_pConfig->riotContribution > 0;
}

bool Pop::IsSpecialist() const
{
    return !m_pConfig->bCanWorkTile && m_pConfig->riotContribution == 0;
}

int Pop::GetRiotContribution() const
{
    return m_pConfig->riotContribution;
}

int Pop::GetGoldenAgeContribution() const
{
    return m_pConfig->goldenAgeContribution;
}

void Pop::SetTileId(int tileId)
{
    m_tileId = tileId;
}

int Pop::GetTileId() const
{
    return m_tileId;
}

PopProduction_t Pop::GetProduction(const PopProduction_t& tileResources) const
{
    const PopTileMultipliers_t& m = m_pConfig->tileMultipliers;
    const PopGeneration_t& g = m_pConfig->generation;

    return PopProduction_t{
        static_cast<int>(std::round(tileResources.nutrients * m.nutrients)) + g.nutrients,
        static_cast<int>(std::round(tileResources.energy    * m.energy))    + g.energy,
        static_cast<int>(std::round(tileResources.minerals  * m.minerals))  + g.minerals,
        g.econ,
        g.labs,
        g.psych
    };
}

} // namespace ac
