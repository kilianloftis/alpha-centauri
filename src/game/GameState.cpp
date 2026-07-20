#include "game/GameState.h"

#include "game/GameSettings.h"
#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/Military.h"
#include "game/faction/DiplomacyManager.h"
#include "game/faction/UnitManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "game/effects/TileEffectsContext.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "lib/EventBus.h"
#include <stdexcept>

namespace ac
{

// One less than the first playable year: TurnStart increments at the start of every turn,
// so the first Advance lands on 2100.
static constexpr int k_StartingMissionYear = 2099;

GameState::GameState(std::unique_ptr<WorldMap> pWorldMap,
                     const ImprovementRegistry& rImprovements,
                     const UnitComponentRegistry* pUnitComponents,
                     GameSettings& rSettings)
    : m_missionYear(k_StartingMissionYear)
    , m_rSettings(rSettings)
    , m_pEventBus(std::make_unique<EventBus>())
    , m_worldMap(std::move(pWorldMap))
    , m_pDiplomacy(std::make_unique<DiplomacyManager>())
    , m_secretProjectAvailability(*this)
{
    if (!m_worldMap)
    {
        throw std::invalid_argument("GameState: pWorldMap is null");
    }
    // UnitPositionIndex owns position only; visibility rebuild stays on Faction (same
    // pattern as create/destroy in UnitManager, and OnUnitDestroyed side effects here).
    m_worldMap->GetUnitPositions().OnUnitMoved.Connect([](Unit& rMoved)
    {
        rMoved.GetFaction().RebuildVisibility();
    });
    m_pTileEffects = std::make_unique<TileEffectsContext>(*m_worldMap, rImprovements, pUnitComponents);
    m_pMoveCosts = std::make_unique<MoveCostCalculator>(rImprovements);
    m_pSteps = std::make_unique<StepEvaluator>(*m_worldMap, *m_pTileEffects);
    m_pPathfinder = std::make_unique<Pathfinder>(*m_pMoveCosts, *m_pSteps, *m_worldMap);
    m_pUnitOrderExecutor = std::make_unique<UnitOrderExecutor>(
        *m_pMoveCosts, *m_pSteps, *m_worldMap, *m_pTileEffects, *m_pPathfinder);
}

GameState::~GameState() = default;

GameSettings& GameState::GetSettings()
{
    return m_rSettings;
}

const GameSettings& GameState::GetSettings() const
{
    return m_rSettings;
}

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

EventBus& GameState::GetEventBus()
{
    return *m_pEventBus;
}

const EventBus& GameState::GetEventBus() const
{
    return *m_pEventBus;
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
        // Lazy view, not a copy: the source is Faction's cached pool (long-lived), and this
        // is its only use, right below.
        auto worldEffects =
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
    pFaction->BindWorldMap(*m_worldMap);
    pFaction->SetOnBaseListChanged([this]() { RebuildTerritory(); });
    // Drop destroyed units from every faction's contact-reveal set (address reuse safety).
    pFaction->GetUnitManager().OnUnitDestroyed.Connect([this](Unit& rDestroyed)
    {
        for (Faction& rFaction : Factions())
        {
            rFaction.GetRevealedUnits().Forget(rDestroyed);
        }
    });
    m_factions.push_back(std::move(pFaction));
    return *m_factions.back();
}

int GameState::GetNumFactions() const
{
    return static_cast<int>(m_factions.size());
}

const Faction* GameState::GetPlayerFaction() const
{
    for (const auto& pFaction : m_factions)
    {
        if (pFaction->IsPlayerControlled())
        {
            return pFaction.get();
        }
    }
    return nullptr;
}

Faction* GameState::GetPlayerFaction()
{
    for (const auto& pFaction : m_factions)
    {
        if (pFaction->IsPlayerControlled())
        {
            return pFaction.get();
        }
    }
    return nullptr;
}

DiplomacyManager& GameState::GetDiplomacy()
{
    return *m_pDiplomacy;
}

const DiplomacyManager& GameState::GetDiplomacy() const
{
    return *m_pDiplomacy;
}

FactionId_t GameState::AllocateFactionId()
{
    return m_factionIdAllocator.Allocate();
}

int GameState::AllocateBaseId()
{
    return m_baseIdAllocator.Allocate();
}

UnitId_t GameState::AllocateUnitId()
{
    return m_unitIdAllocator.Allocate();
}

WorldMap& GameState::GetWorldMap()
{
    return *m_worldMap;
}

const WorldMap& GameState::GetWorldMap() const
{
    return *m_worldMap;
}

BaseManager* GameState::FindBaseAt(int tileX, int tileY)
{
    for (Faction& rFaction : Factions())
    {
        for (BaseManager& rBase : rFaction.Bases())
        {
            if (rBase.GetX() == tileX && rBase.GetY() == tileY)
            {
                return &rBase;
            }
        }
    }
    return nullptr;
}

const BaseManager* GameState::FindBaseAt(int tileX, int tileY) const
{
    for (const Faction& rFaction : Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            if (rBase.GetX() == tileX && rBase.GetY() == tileY)
            {
                return &rBase;
            }
        }
    }
    return nullptr;
}

TileEffectsContext& GameState::GetTileEffects()
{
    return *m_pTileEffects;
}

const TileEffectsContext& GameState::GetTileEffects() const
{
    return *m_pTileEffects;
}

Pathfinder& GameState::GetPathfinder()
{
    return *m_pPathfinder;
}

const Pathfinder& GameState::GetPathfinder() const
{
    return *m_pPathfinder;
}

UnitOrderExecutor& GameState::GetUnitOrderExecutor()
{
    return *m_pUnitOrderExecutor;
}

void GameState::RebuildTerritory()
{
    std::vector<const BaseManager*> bases;
    for (const Faction& rFaction : Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
    }
    m_worldMap->GetTerritory().Rebuild(*m_worldMap, bases);
}

const SecretProjectAvailabilityCalculator& GameState::GetSecretProjectAvailability() const
{
    return m_secretProjectAvailability;
}

} // namespace ac
