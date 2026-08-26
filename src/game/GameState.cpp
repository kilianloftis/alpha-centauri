#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/buildings/BuildingManager.h"

#include "game/GameSettings.h"
#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/Military.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/UnitManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "game/effects/TileEffectsContext.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/ProbeActionExecutor.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "game/units/BaseConquestEffects.h"
#include "game/units/InterceptRules.h"
#include "game/GameDataContext.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "lib/EventBus.h"
#include <algorithm>
#include <random>
#include <stdexcept>

namespace ac
{

GameState::GameState(std::unique_ptr<WorldMap> pWorldMap,
                     const ImprovementRegistry& rImprovements,
                     const UnitComponentRegistry* pUnitComponents,
                     GameSettings& rSettings,
                     const MoraleCalculator& rMorale,
                     const TileYieldRulesConfig_t& rYieldRules,
                     uint32_t rngSeed)
    : m_missionYear(k_StartingMissionYear)
    , m_rSettings(rSettings)
    , m_rMorale(rMorale)
    , m_visibilitySettingsChanged(rSettings.OnVisibilityChanged.ConnectScoped(
          [this]() { OnVisibilitySettingsChanged_(); }))
    , m_pEventBus(std::make_unique<EventBus>())
    , m_worldMap(std::move(pWorldMap))
    , m_pDiplomacy(std::make_unique<DiplomacyLedger>())
    , m_pDiplomaticActionExecutor(std::make_unique<DiplomaticActionExecutor>())
    , m_rng(rngSeed)
    , m_secretProjectAvailability(*this)
{
    if (!m_worldMap)
    {
        throw std::invalid_argument("GameState: pWorldMap is null");
    }
    m_pTileEffects = std::make_unique<TileEffectsContext>(*m_worldMap, rImprovements,
                                                          pUnitComponents, rYieldRules);
    m_pMoveCosts = std::make_unique<MoveCostCalculator>(rImprovements);
    m_pSteps = std::make_unique<StepEvaluator>(*m_worldMap, *m_pTileEffects);
    m_pPathfinder = std::make_unique<Pathfinder>(*m_pMoveCosts, *m_pSteps, *m_worldMap);
    m_pFirstContact = std::make_unique<FirstContactResolver>(
        *m_pDiplomacy, m_factions);
    // UnitPositionIndex owns position only; visibility rebuild stays on Faction (same
    // pattern as create/destroy in UnitManager, and OnUnitDestroyed side effects here).
    m_worldMap->GetUnitPositions().OnUnitMoved.Connect([this](Unit& rMoved)
    {
        rMoved.GetFaction().RebuildVisibility();
        // Stationary factions whose vision already covers the new tile must also meet.
        m_pFirstContact->ConsiderUnit(rMoved);
    });
    // Passing *this is safe here: the executor only stores the pointer and never calls back
    // during GameState's own construction (same contract as m_secretProjectAvailability).
    m_pUnitOrderExecutor = std::make_unique<UnitOrderExecutor>(
        *m_pMoveCosts, *m_pSteps, *m_worldMap, *m_pTileEffects, *m_pPathfinder, m_rMorale,
        m_rng, this);
    m_pProbeActions = std::make_unique<ProbeActionExecutor>(*m_worldMap, m_rMorale, m_rng);
}

GameState::~GameState() = default;

const MoraleCalculator& GameState::GetMoraleCalculator() const
{
    return m_rMorale;
}

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

int GameState::GetYearsSinceFirstPlayableYear() const
{
    return std::max(0, m_missionYear - k_FirstPlayableMissionYear);
}

std::mt19937& GameState::GetRng()
{
    return m_rng;
}

const std::mt19937& GameState::GetRng() const
{
    return m_rng;
}

EventBus& GameState::GetEventBus()
{
    return *m_pEventBus;
}

const EventBus& GameState::GetEventBus() const
{
    return *m_pEventBus;
}

PlayerInteractionQueue& GameState::GetPlayerInteractions()
{
    return m_playerInteractions;
}

const PlayerInteractionQueue& GameState::GetPlayerInteractions() const
{
    return m_playerInteractions;
}

uint64_t GameState::GetWorldCompositionStamp(const Faction& rFor) const
{
    // Mix peer local-pool versions and council revision. Using local versions (not composed)
    // keeps council extras from feeding back into the stamp when peers also compose.
    auto mix = [](uint64_t a, uint64_t b) -> uint64_t
    {
        return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    };

    uint64_t stamp = 0;
    for (const auto& pFaction : m_factions)
    {
        if (pFaction.get() == &rFor)
        {
            continue;
        }
        stamp = mix(stamp, pFaction->GetLocalEffectsVersion());
    }
    if (m_pCouncil)
    {
        stamp = mix(stamp, m_pCouncil->GetRevision().Get());
    }
    return stamp;
}

std::vector<ActiveEffect_t> GameState::CollectWorldExtras(const Faction& rFor) const
{
    std::vector<ActiveEffect_t> result;
    for (const auto& pFaction : m_factions)
    {
        if (pFaction.get() == &rFor)
        {
            continue;
        }
        // Peer *local* pool only: composed GetActiveEffects would re-emit council extras.
        auto worldEffects =
            FilterByScope(pFaction->GetLocalActiveEffects().effects, EffectScope_t::WorldGlobal);
        result.insert(result.end(), worldEffects.begin(), worldEffects.end());
    }
    if (m_pCouncil)
    {
        // CouncilEffects keeps config addresses stable across rebuilds; revision bumps still
        // force composed-pool caches to pick up the new active set.
        const std::vector<ActiveEffect_t>& councilWorld = m_pCouncil->CollectWorldEffects();
        result.insert(result.end(), councilWorld.begin(), councilWorld.end());
        const std::vector<ActiveEffect_t>& councilFaction = m_pCouncil->CollectFactionEffects(rFor);
        result.insert(result.end(), councilFaction.begin(), councilFaction.end());
    }
    return result;
}

Faction& GameState::AddFaction(std::unique_ptr<Faction> pFaction)
{
    if (!pFaction)
    {
        throw std::invalid_argument("GameState::AddFaction: pFaction is null");
    }
    // Contract: pFaction must have been constructed against this session's WorldMap. The old
    // AddFaction called BindWorldMap and thereby *made* that true; now the map is a Faction
    // constructor dependency, so the caller establishes it (Engine passes
    // m_pGameState->GetWorldMap()). A mismatch resolves Sensor vision and territory ownership
    // from the wrong world while coordinate-based unit/base reveal keeps working, so it shows
    // up as subtly wrong visibility rather than an obvious failure.
    //
    // Not enforced with a throw here: the test fixtures own their world map and lend a
    // GameState to compose effects against, so identity would require handing map ownership to
    // GameState — a fixture redesign with destruction-order hazards (factions and bases hold
    // Tile& into the map) that is out of this package's scope. See
    // docs/full-review-fix-prompts/04-composition-root-and-deps.md.
    // Register first, attach second. The faction must already be in m_factions before any
    // observer runs: AttachToSession_ ends with a visibility/territory sweep, and both
    // ConsiderObserver and RebuildTerritory iterate Factions(). Wiring before the push (the
    // previous order) meant a faction arriving with bases already on the map — load-game, or
    // any future runtime creation — was never scanned for contact or territory.
    m_factions.push_back(std::move(pFaction));
    Faction& rAdded = *m_factions.back();
    AttachToSession_(rAdded);
    return rAdded;
}

void GameState::AttachToSession_(Faction& rFaction)
{
    // The whole session wiring for one faction, in one place. World map and settings are
    // constructor dependencies (see Faction's constructor); what remains is the container
    // back-pointer and the observers, which necessarily close over this GameState.
    rFaction.BindWorldEffects(*this);
    rFaction.BindGameState(*this);
    rFaction.SetOnBaseListChanged([this]()
    {
        RebuildTerritory();
        // A new base may sit inside another faction's existing vision cone.
        for (Faction& rObserver : Factions())
        {
            m_pFirstContact->ConsiderObserver(rObserver);
        }
    });
    rFaction.SetOnVisibilityRebuilt([this](Faction& rObserver)
    {
        m_pFirstContact->ConsiderObserver(rObserver);
    });
    // Drop destroyed units from every faction's contact-reveal set (address reuse safety).
    rFaction.GetUnitManager().OnUnitDestroyed.Connect([this](Unit& rDestroyed)
    {
        for (Faction& rOther : Factions())
        {
            rOther.GetRevealedUnits().Forget(rDestroyed);
        }
    });
    rFaction.GetUnitManager().OnUnitCreated.Connect([this](Unit& rCreated)
    {
        // Created into another faction's existing vision (no move event).
        m_pFirstContact->ConsiderUnit(rCreated);
    });
    rFaction.GetUnitManager().OnUnitAdopted.Connect([this](Unit& rAdopted)
    {
        // Transfer is not a birth (see UnitManager::OnUnitAdopted), but the unit now sits in
        // observers' vision under a faction they may not have met yet — same first-contact
        // scan as a genuine creation.
        m_pFirstContact->ConsiderUnit(rAdopted);
    });

    // Catch up on anything the faction already owns. A freshly-constructed faction has no
    // bases and no units, so this is a no-op for the new-game path; it is what makes an
    // already-populated faction correct.
    RebuildTerritory();
    rFaction.RebuildVisibility();
    // Contact is symmetric, so the sweep must be too. rFaction.RebuildVisibility() only asks
    // "what can the newcomer see"; an incumbent whose vision already covers the newcomer's
    // bases would never be re-examined, and would stay unaware of a faction sitting inside its
    // own sight radius until some unrelated later event. Same full sweep the base-list handler
    // above performs, for the same reason.
    for (Faction& rObserver : Factions())
    {
        m_pFirstContact->ConsiderObserver(rObserver);
    }
}

int GameState::GetNumFactions() const
{
    return static_cast<int>(m_factions.size());
}

Faction* GameState::FindFaction(FactionId_t factionId)
{
    for (auto& pFaction : m_factions)
    {
        if (pFaction->GetFactionId() == factionId)
        {
            return pFaction.get();
        }
    }
    return nullptr;
}

const Faction* GameState::FindFaction(FactionId_t factionId) const
{
    for (const auto& pFaction : m_factions)
    {
        if (pFaction->GetFactionId() == factionId)
        {
            return pFaction.get();
        }
    }
    return nullptr;
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

DiplomacyLedger& GameState::GetDiplomacyLedger()
{
    return *m_pDiplomacy;
}

const DiplomacyLedger& GameState::GetDiplomacyLedger() const
{
    return *m_pDiplomacy;
}

DiplomaticActionExecutor& GameState::GetDiplomaticActionExecutor()
{
    return *m_pDiplomaticActionExecutor;
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
            const Tile& rTile = rBase.GetTile();
            if (rTile.GetX() == tileX && rTile.GetY() == tileY)
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
            const Tile& rTile = rBase.GetTile();
            if (rTile.GetX() == tileX && rTile.GetY() == tileY)
            {
                return &rBase;
            }
        }
    }
    return nullptr;
}

std::optional<CombatResult_t> GameState::TryInterceptAttack(
    Unit& rAttacker, Unit& rDefender, TileEffectsContext& rTileEffects, std::mt19937& rRng)
{
    return ac::TryInterceptAttack(*this, rAttacker, rDefender, rTileEffects, rRng);
}

BaseConquestResult_t GameState::ResolvePostCombatBaseConquest(
    Unit& rAttacker, const Tile& rDefenderTile, const GameDataContext& rDataContext,
    std::mt19937& rRng)
{
    return ac::ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, *this, rDataContext, rRng);
}

BaseConquestResult_t GameState::ResolveBaseEntryConquest(
    Unit& rMover, const GameDataContext& rDataContext, std::mt19937& rRng)
{
    return ac::ResolveBaseEntryConquest(rMover, *this, rDataContext, rRng);
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

ProbeActionExecutor& GameState::GetProbeActions()
{
    return *m_pProbeActions;
}

const ProbeActionExecutor& GameState::GetProbeActions() const
{
    return *m_pProbeActions;
}

FirstContactResolver& GameState::GetFirstContactResolver()
{
    return *m_pFirstContact;
}

const FirstContactResolver& GameState::GetFirstContactResolver() const
{
    return *m_pFirstContact;
}

void GameState::CreatePlanetaryCouncil(const CouncilProposalRegistry& rRegistry,
                                       const CouncilRulesConfig_t& rRules)
{
    if (m_pCouncil)
    {
        throw std::logic_error("GameState::CreatePlanetaryCouncil: council already exists");
    }

    std::vector<Faction*> members;
    for (Faction& rFaction : Factions())
    {
        if (rFaction.GetDefinition().identity.participatesInCouncil)
        {
            members.push_back(&rFaction);
        }
    }
    m_pCouncil = std::make_unique<PlanetaryCouncil>(rRegistry, rRules, std::move(members));
}

PlanetaryCouncil* GameState::GetPlanetaryCouncil()
{
    return m_pCouncil.get();
}

const PlanetaryCouncil* GameState::GetPlanetaryCouncil() const
{
    return m_pCouncil.get();
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

void GameState::RazeBase(BaseManager& rBase)
{
    for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
    {
        if (pBuilding && pBuilding->bIsSecretProject)
        {
            MarkSecretProjectDestroyed(pBuilding->id);
        }
    }
    rBase.GetFaction().ExtractBase(rBase.GetBaseId());
}

void GameState::MarkSecretProjectDestroyed(const std::string& buildingId)
{
    m_destroyedSecretProjects.insert(buildingId);
}

bool GameState::IsSecretProjectDestroyed(const std::string& buildingId) const
{
    return m_destroyedSecretProjects.contains(buildingId);
}

std::vector<OrbitalCensusEntry_t> GameState::GetOrbitalCensus() const
{
    return BuildOrbitalCensus(*this);
}

int GameState::CountOrbitalBuildings(FactionId_t factionId, const BuildingId_t& buildingId) const
{
    return CountFactionOrbitalBuildings(*this, factionId, buildingId);
}

std::vector<OrbitalAttackerOption_t> GameState::ListReadyOrbitalAttackers(
    const Faction& rFaction) const
{
    return ::ac::ListReadyOrbitalAttackers(rFaction, m_missionYear);
}

OrbitalAttackResult_t GameState::TryAttackSatellite(Faction& rAttacker,
                                                    Faction& rDefender,
                                                    const BuildingId_t& attackerBuildingId,
                                                    const BuildingId_t& targetOrbitalBuildingId)
{
    return ::ac::TryAttackSatellite(*this, rAttacker, rDefender, attackerBuildingId,
                                    targetOrbitalBuildingId, m_rng);
}

void GameState::OnVisibilitySettingsChanged_()
{
    for (Faction& rFaction : Factions())
    {
        rFaction.RebuildVisibility();
    }
}

} // namespace ac
