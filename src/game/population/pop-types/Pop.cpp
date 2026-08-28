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
    return m_pConfig->derivedClass == PopClass_t::Drone;
}

bool Pop::IsTalent() const
{
    return m_pConfig->derivedClass == PopClass_t::Talent;
}

bool Pop::IsPlainWorker() const
{
    return m_pConfig->derivedClass == PopClass_t::PlainWorker;
}

bool Pop::IsPlayerChoiceType() const
{
    return m_pConfig->bPlayerAssignable && !m_pConfig->bIsDefault;
}

bool Pop::IsInCompositionGraph() const
{
    return m_pConfig->derivedClass != PopClass_t::Outside;
}

bool Pop::ParticipatesInComposition() const
{
    return IsInCompositionGraph() && !IsPlayerChoiceType();
}

MoodWeights_t Pop::GetMoodWeights() const
{
    // Same ThisPop lane as ApplyTileMultipliers: resolved off this pop's own type, never
    // through ResolveBaseStat, so summing mood never builds virtual citizens.
    const std::vector<ActiveEffect_t> popEffects = CollectScoped_(EffectScope_t::ThisPop);

    auto resolve = [&](StatId_t statId) -> int
    {
        return FinalizeResolvedStat(
            ResolveStatModifiers(FilterByStatId(popEffects, statId), SeedFor(statId)).total);
    };

    return MoodWeights_t{resolve(StatId_t::RiotWeight), resolve(StatId_t::GoldenAgeWeight)};
}

int Pop::GetDroneWeight() const
{
    return m_pConfig->droneWeight;
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

std::vector<ActiveEffect_t> Pop::CollectScoped_(EffectScope_t scope) const
{
    // Materialized, not a view: CollectPopEffects(...) is a temporary, and every caller below
    // resolves several stats against the result, so the list has to outlive this statement.
    const std::vector<ActiveEffect_t> popEffects = CollectPopEffects(*m_pConfig);
    auto scopedView = FilterByScope(popEffects, scope);
    return std::vector<ActiveEffect_t>(scopedView.begin(), scopedView.end());
}

TileResources_t Pop::ApplyTileMultipliers(const TileResources_t& resources) const
{
    // Only ThisPop-scoped effects (tile multipliers) apply here — ThisBase-scoped flat
    // generation bonuses are resolved separately via CollectFromPops/ResourceManager.
    const std::vector<ActiveEffect_t> tileEffects = CollectScoped_(EffectScope_t::ThisPop);

    auto scaleByMultiplier = [&](StatId_t statId, int rawValue) -> int
    {
        return FinalizeResolvedStat(
            ResolveStatModifiers(FilterByStatId(tileEffects, statId),
                                 static_cast<double>(rawValue)).total);
    };

    return TileResources_t{
        scaleByMultiplier(StatId_t::Nutrients, resources.nutrients),
        scaleByMultiplier(StatId_t::Energy, resources.energy),
        scaleByMultiplier(StatId_t::Minerals, resources.minerals)
    };
}

SpecialistOutput_t Pop::GetSpecialistOutput() const
{
    const std::vector<ActiveEffect_t> flatEffects = CollectScoped_(EffectScope_t::ThisBase);

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
