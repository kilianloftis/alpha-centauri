#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/resources/ResourceManager.h"
#include <cmath>

namespace ac
{

Pop::Pop(const PopTypeConfig& rConfig, int id)
    : m_pConfig(&rConfig)
    , m_id(id)
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

bool Pop::IsPlayerAssignable() const
{
    return m_pConfig->bPlayerAssignable;
}

int Pop::GetRiotContribution() const
{
    return m_pConfig->riotContribution;
}

int Pop::GetGoldenAgeContribution() const
{
    return m_pConfig->goldenAgeContribution;
}

int Pop::GetId() const
{
    return m_id;
}

void Pop::SetId(int id)
{
    m_id = id;
}

void Pop::SetTileId(int tileId)
{
    m_tileId = tileId;
}

int Pop::GetTileId() const
{
    return m_tileId;
}

TileResources_t Pop::ApplyTileMultipliers(const TileResources_t& resources) const
{
    const PopTileMultipliers_t& m = m_pConfig->tileMultipliers;
    const PopGeneration_t& g = m_pConfig->generation;

    return TileResources_t{
        static_cast<int>(std::round(resources.nutrients * m.nutrients)) + g.nutrients,
        static_cast<int>(std::round(resources.energy    * m.energy))    + g.energy,
        static_cast<int>(std::round(resources.minerals  * m.minerals))  + g.minerals
    };
}

SpecialistOutput_t Pop::GetSpecialistOutput() const
{
    const PopGeneration_t& g = m_pConfig->generation;

    return SpecialistOutput_t{
        g.econ,
        g.labs,
        g.psych
    };
}

} // namespace ac
