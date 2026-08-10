#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include <cmath>
#include <stdexcept>

namespace ac
{

Pop::Pop(const PopTypeConfig_t& rConfig)
    : m_pConfig(&rConfig)
{
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
    return m_pConfig->role == PopRole_t::Drone;
}

bool Pop::IsTalent() const
{
    return m_pConfig->role == PopRole_t::Talent;
}

bool Pop::IsSpecialist() const
{
    return m_pConfig->role == PopRole_t::Specialist;
}

bool Pop::IsPlainWorker() const
{
    return m_pConfig->role == PopRole_t::Worker;
}

int Pop::GetRiotContribution() const
{
    return m_pConfig->riotContribution;
}

void Pop::Convert(const PopTypeConfig_t& rConfig)
{
    m_pConfig = &rConfig;
    if (!m_pConfig->bCanWorkTile)
    {
        // A non-worker holds no claim; releasing it frees the tile in WorkedTileIndex.
        m_tileClaim = WorkedTileClaim{};
    }
}

void Pop::SetTileClaim(WorkedTileClaim claim)
{
    if (claim.GetTile() && !IsWorker())
    {
        throw std::logic_error("Pop::SetTileClaim: pop type cannot work tiles");
    }
    m_tileClaim = std::move(claim);
}

const Tile* Pop::GetTile() const
{
    return m_tileClaim.GetTile();
}

bool Pop::IsUserAssigned() const
{
    return m_tileClaim.IsUserAssigned();
}

TileResources_t Pop::ApplyTileMultipliers(const TileResources_t& resources) const
{
    // Only ThisPop-scoped effects (tile multipliers) apply here — ThisBase-scoped flat
    // generation bonuses are resolved separately via CollectFromPops/ResourceManager.
    // Bind the source to a named local before filtering: CollectPopEffects(...) is
    // otherwise a temporary, and tileEffects must survive past this statement (used by
    // the lambda below, invoked once per resource).
    const std::vector<ActiveEffect_t> popEffects = CollectPopEffects(*m_pConfig);
    auto tileEffectsView = FilterByScope(popEffects, EffectScope_t::ThisPop);
    const std::vector<ActiveEffect_t> tileEffects(tileEffectsView.begin(), tileEffectsView.end());

    auto scaleByMultiplier = [&](StatId_t statId, int rawValue) -> int
    {
        const StatBreakdown_t breakdown =
            ResolveStatModifiers(FilterByStatId(tileEffects, statId), static_cast<double>(rawValue));
        return FinalizeResolvedStat(breakdown.total);
    };

    return TileResources_t{
        scaleByMultiplier(StatId_t::Nutrients, resources.nutrients),
        scaleByMultiplier(StatId_t::Energy, resources.energy),
        scaleByMultiplier(StatId_t::Minerals, resources.minerals)
    };
}

SpecialistOutput_t Pop::GetSpecialistOutput() const
{
    // Same reasoning as ApplyTileMultipliers above: materialize before the lambda reuses it.
    const std::vector<ActiveEffect_t> popEffects = CollectPopEffects(*m_pConfig);
    auto flatEffectsView = FilterByScope(popEffects, EffectScope_t::ThisBase);
    const std::vector<ActiveEffect_t> flatEffects(flatEffectsView.begin(), flatEffectsView.end());

    auto resolveFlat = [&](StatId_t statId) -> int
    {
        return FinalizeResolvedStat(
            ResolveStatModifiers(FilterByStatId(flatEffects, statId), SeedFor(statId)).total);
    };

    return SpecialistOutput_t{
        resolveFlat(StatId_t::Econ),
        resolveFlat(StatId_t::Labs),
        resolveFlat(StatId_t::Psych)
    };
}

} // namespace ac
