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

GameState::GameState()
    : m_missionYear(2100)
{
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

std::vector<std::unique_ptr<Faction>>& GameState::GetFactions()
{
    return m_factions;
}

const std::vector<std::unique_ptr<Faction>>& GameState::GetFactions() const
{
    return m_factions;
}

std::vector<ActiveEffect_t> GameState::CollectWorldEffects(const Faction& rExclude) const
{
    std::vector<ActiveEffect_t> result;
    for (const auto& pFaction : m_factions)
    {
        if (!pFaction || pFaction.get() == &rExclude)
        {
            continue;
        }
        const std::vector<ActiveEffect_t> worldEffects =
            FilterByScope(CollectActiveEffects(*pFaction), EffectScope_t::WorldGlobal);
        result.insert(result.end(), worldEffects.begin(), worldEffects.end());
    }
    return result;
}

void GameState::AddFaction(std::unique_ptr<Faction> pFaction)
{
    m_factions.push_back(std::move(pFaction));
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

WorldMap* GameState::GetWorldMap()
{
    return m_worldMap.get();
}

const WorldMap* GameState::GetWorldMap() const
{
    return m_worldMap.get();
}

void GameState::SetWorldMap(std::unique_ptr<WorldMap> pWorldMap)
{
    m_worldMap = std::move(pWorldMap);
}

void GameState::InitTileEffects(const ImprovementRegistry& rImprovements,
                                const UnitComponentRegistry* pUnitComponents)
{
    if (!m_worldMap)
    {
        throw std::runtime_error("GameState::InitTileEffects: WorldMap must be set before initializing tile effects");
    }
    m_pTileEffects = std::make_unique<TileEffectsContext>(*m_worldMap, rImprovements, pUnitComponents);
}

TileEffectsContext& GameState::GetTileEffects()
{
    if (!m_pTileEffects)
    {
        throw std::runtime_error("GameState::GetTileEffects: tile effects not initialized — call InitTileEffects first");
    }
    return *m_pTileEffects;
}

const TileEffectsContext& GameState::GetTileEffects() const
{
    if (!m_pTileEffects)
    {
        throw std::runtime_error("GameState::GetTileEffects: tile effects not initialized — call InitTileEffects first");
    }
    return *m_pTileEffects;
}

UnitOrderExecutor& GameState::GetUnitOrderExecutor()
{
    return m_unitOrderExecutor;
}

} // namespace ac
