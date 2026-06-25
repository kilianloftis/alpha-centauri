#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/Tile.h"
#include <cmath>

namespace ac
{

Pop::Pop(const PopTypeConfig_t& rConfig)
    : m_pConfig(&rConfig)
    , m_pTile(nullptr)
    , m_bUserAssigned(false)
{
}

Pop::~Pop()
{
    if (m_pTile)
    {
        m_pTile->SetWorked(false);
    }
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

void Pop::Convert(const PopTypeConfig_t& rConfig)
{
    m_pConfig = &rConfig;
    if (!m_pConfig->bCanWorkTile)
    {
        SetTile(nullptr);
    }
}

void Pop::SetTile(const Tile* pTile)
{
    if (m_pTile == pTile)
    {
        return;
    }
    if (m_pTile)
    {
        m_pTile->SetWorked(false);
    }
    m_pTile = pTile;
    if (m_pTile)
    {
        m_pTile->SetWorked(true);
    }
    else
    {
        m_bUserAssigned = false;
    }
}

const Tile* Pop::GetTile() const
{
    return m_pTile;
}

void Pop::SetUserAssigned(bool bUserAssigned)
{
    m_bUserAssigned = bUserAssigned;
}

bool Pop::IsUserAssigned() const
{
    return m_bUserAssigned;
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
