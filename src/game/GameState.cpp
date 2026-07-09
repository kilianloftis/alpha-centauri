#include "game/GameState.h"

#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/Military.h"
#include "game/faction/Diplomacy.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "lib/effects/TileEffectsContext.h"
#include <stdexcept>

namespace ac
{

static constexpr int k_startingMissionYear = 2100;

GameState::GameState(std::unique_ptr<WorldMap> pWorldMap,
                     const ImprovementRegistry& rImprovements,
                     const UnitComponentRegistry* pUnitComponents)
    : m_missionYear(k_startingMissionYear)
    , m_worldMap(std::move(pWorldMap))
{
    if (!m_worldMap)
    {
        throw std::invalid_argument("GameState: pWorldMap is null");
    }
    m_pTileEffects = std::make_unique<TileEffectsContext>(*m_worldMap, rImprovements, pUnitComponents);
}

GameState::~GameState() = default;

int GameState::GetMissionYear() const
{
    return m_missionYear;
}

void GameState::SetMissionYear(int year)
{
    m_missionYear = year;
}

void GameState::IncrementMissionYear()
{
    ++m_missionYear;
}

std::vector<ActiveEffect_t> GameState::CollectWorldEffects(const Faction& rExclude) const
{
    std::vector<ActiveEffect_t> result;
    for (const auto& pFaction : m_factions)
    {
        if (pFaction.get() == &rExclude)
        {
            continue;
        }
        const std::vector<ActiveEffect_t> worldEffects =
            FilterByScope(pFaction->GetActiveEffects().effects, EffectScope_t::WorldGlobal);
        result.insert(result.end(), worldEffects.begin(), worldEffects.end());
    }
    return result;
}

Faction& GameState::AddFaction(std::unique_ptr<Faction> pFaction)
{
    if (!pFaction)
    {
        throw std::invalid_argument("GameState::AddFaction: pFaction is null");
    }
    m_factions.push_back(std::move(pFaction));
    return *m_factions.back();
}

int GameState::GetNumFactions() const
{
    return static_cast<int>(m_factions.size());
}

const Faction* GameState::GetPlayerFaction() const
{
    return m_factions.empty() ? nullptr : m_factions[0].get();
}

Faction* GameState::GetPlayerFaction()
{
    return m_factions.empty() ? nullptr : m_factions[0].get();
}

WorldMap& GameState::GetWorldMap()
{
    return *m_worldMap;
}

const WorldMap& GameState::GetWorldMap() const
{
    return *m_worldMap;
}

TileEffectsContext& GameState::GetTileEffects()
{
    return *m_pTileEffects;
}

const TileEffectsContext& GameState::GetTileEffects() const
{
    return *m_pTileEffects;
}

UnitOrderExecutor& GameState::GetUnitOrderExecutor()
{
    return m_unitOrderExecutor;
}

} // namespace ac
