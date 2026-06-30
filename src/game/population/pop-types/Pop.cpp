#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/Tile.h"
#include "lib/effects/ActiveEffect.h"
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

const PopTypeConfig_t& Pop::GetConfig() const
{
    return *m_pConfig;
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
    // Only ThisPop-scoped effects (tile multipliers) apply here — ThisBase-scoped flat
    // generation bonuses are resolved separately via CollectFromPops/ResourceManager.
    const std::vector<ActiveEffect_t> tileEffects =
        FilterByScope(CollectPopEffects(*m_pConfig), EffectScope_t::ThisPop);

    auto scaleByMultiplier = [&](StatId statId, int rawValue) -> int
    {
        const StatBreakdown_t breakdown =
            ResolveStatModifiers(FilterByStatId(tileEffects, statId), static_cast<double>(rawValue));
        return static_cast<int>(std::round(breakdown.total));
    };

    return TileResources_t{
        scaleByMultiplier(StatId::Nutrients, resources.nutrients),
        scaleByMultiplier(StatId::Energy, resources.energy),
        scaleByMultiplier(StatId::Minerals, resources.minerals)
    };
}

SpecialistOutput_t Pop::GetSpecialistOutput() const
{
    const std::vector<ActiveEffect_t> flatEffects =
        FilterByScope(CollectPopEffects(*m_pConfig), EffectScope_t::ThisBase);

    auto resolveFlat = [&](StatId statId) -> int
    {
        return static_cast<int>(ResolveStatModifiers(FilterByStatId(flatEffects, statId)).total);
    };

    return SpecialistOutput_t{
        resolveFlat(StatId::Econ),
        resolveFlat(StatId::Labs),
        resolveFlat(StatId::Psych)
    };
}

} // namespace ac
