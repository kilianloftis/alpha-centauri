#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/production/ProductionRules.h"
#include "game/units/Military.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/PauseOnEventsConfig.h"

#include "game/GameSettings.h"
#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/diplomacy/DiplomaticActionExecutor.h"
#include "game/units/UnitManager.h"
#include "game/research/ResearchManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/terraform/TerraformRules.h"
#include "game/units/UnitAttackExecutor.h"
#include "game/units/UnitMoveExecutor.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/probes/ProbeActionExecutor.h"
#include "game/movement/Pathfinder.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitComponentConfig.h"
#include "game/base/BaseConquestMutations.h"
#include "game/combat/InterceptMutations.h"
#include "game/GameDataContext.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/worldgen/WorldGenDecorationConfig.h"
#include "lib/EventBus.h"
#include <algorithm>
#include <functional>
#include <random>
#include <stdexcept>

namespace ac
{

namespace
{

void ApplyBuildingCompleteEffects_(GameState& rGameState, BaseManager& rBase,
                                   const BuildingConfig_t& rBuilding)
{
    TriggeredEffectContext_t context(rGameState, rBase);
    context.populationFloor = CompletionPopulationFloor();
    ApplyTriggeredEffects(rBuilding.onCompleteEffects, context);
    // A copy: a hold link can consume a unit and so change the tile's list.
    const std::vector<Unit*> holders =
        rGameState.GetWorldMap().GetUnitPositions().GetUnitsOnTile(rBase.GetTile());
    for (Unit* pUnit : holders)
    {
        if (pUnit)
        {
            rGameState.ConsiderHoldLink(*pUnit);
        }
    }
}

// Only a produced unit pays completion costs; free spawns never come through here.
void ApplyUnitCompleteEffects_(GameState& rGameState, const ProductionCompleted_t& rCompleted,
                               const UnitDesign& rDesign)
{
    TriggeredEffectContext_t context(rGameState, rCompleted.rBase);
    context.pUnit = rCompleted.pUnit;
    context.populationFloor = CompletionPopulationFloor();
    for (const UnitComponentConfig_t* pComp : rDesign.GetComponents())
    {
        ApplyTriggeredEffects(pComp->onCompleteEffects, context);
    }
}

void ApplyProductionCompleteEffects_(GameState& rGameState, const ProductionCompleted_t& rCompleted)
{
    BaseManager& rBase = rCompleted.rBase;
    if (const BuildingConfig_t* pBuilding =
            rBase.GetBuildingManager().FindBuilding(rCompleted.itemId))
    {
        ApplyBuildingCompleteEffects_(rGameState, rBase, *pBuilding);
        return;
    }
    const IDesign* pDesign = rBase.GetFaction().GetMilitary().GetDesign(rCompleted.itemId);
    if (const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(pDesign))
    {
        ApplyUnitCompleteEffects_(rGameState, rCompleted, *pUnitDesign);
    }
}

void NoticeAtBase_(GameState& rGameState, const BaseManager& rBase, PauseOnEventId_t eventId,
                   std::string title, std::string message)
{
    const TileCoord_t at{rBase.GetTile().GetX(), rBase.GetTile().GetY()};
    EnqueueForPlayer(rGameState,
                     NoticeInteraction_t{eventId, std::move(title), std::move(message), at});
}

void WireMoodNotices_(GameState& rGameState, BaseManager& rBase)
{
    Faction& rFaction = rBase.GetFaction();
    if (!rFaction.IsPlayerControlled())
    {
        return;
    }

    PopulationManager& rPopulation = rBase.GetPopulation();
    rPopulation.OnWillRiot.Connect(
        [&rGameState, &rBase]()
        {
            NoticeAtBase_(rGameState, rBase, PauseOnEventId_t::DroneRiots, "Drone Riots",
                          "Drones threaten to riot at " + rBase.GetName()
                              + ". Adjust specialists or psych before the turn ends.");
        });
    // TODO: should a golden age announce on the verge or only on arrival? Unlike a riot, the player
    // has nothing to avert.
    rPopulation.OnWillGoldenAge.Connect(
        [&rGameState, &rBase]()
        {
            NoticeAtBase_(rGameState, rBase, PauseOnEventId_t::GoldenAgeStarts, "Golden Age",
                          rBase.GetName() + " is on the verge of a Golden Age.");
        });
}

} // namespace

GameState::GameState(std::unique_ptr<WorldMap> pWorldMap,
                     const GameDataContext& rGameData,
                     GameSettings& rSettings,
                     uint32_t rngSeed)
    : m_missionYear(k_StartingMissionYear)
    , m_rSettings(rSettings)
    , m_rGameData(rGameData)
    , m_visibilitySettingsChanged(rSettings.OnVisibilityChanged.ConnectScoped(
          [this]() { OnVisibilitySettingsChanged_(); }))
    , m_pEventBus(std::make_unique<EventBus>())
    , m_worldMap(std::move(pWorldMap))
    , m_pDiplomacy(std::make_unique<DiplomacyLedger>())
    , m_pAtrocities(std::make_unique<AtrocityLedger>())
    , m_pMindControl(std::make_unique<MindControlLedger>())
    , m_pEcology(std::make_unique<EcologyLedger>())
    , m_pDiplomaticActionExecutor(std::make_unique<DiplomaticActionExecutor>())
    , m_rng(rngSeed)
{
    if (!m_worldMap)
    {
        throw std::invalid_argument("GameState: pWorldMap is null");
    }
    if (!rGameData.councilProposalRegistry || !rGameData.councilRules)
    {
        throw std::invalid_argument("GameState: the council proposals and rules are not loaded");
    }
    m_pCouncil = std::make_unique<PlanetaryCouncil>(*rGameData.councilProposalRegistry,
                                                    *rGameData.councilRules);
    m_pCouncilVotes = std::make_unique<CouncilVoteExecutor>(*m_pCouncil);
    m_pTileEffects = std::make_unique<TileEffectsContext>(
        *m_worldMap, *rGameData.improvementRegistry, rGameData.unitComponentRegistry.get(),
        rGameData.tileYieldRules, rGameData.interactionGrids,
        rGameData.worldGenDecorationConfig->moisture);
    m_pTileEffects->BindWorldEffects(*this);
    m_pMoveCosts = std::make_unique<MoveCostRules>(*rGameData.improvementRegistry,
                                                   MovementConstants_t{});
    m_pSteps = std::make_unique<StepEvaluator>(*m_worldMap, *m_pTileEffects);
    m_pPathfinder = std::make_unique<Pathfinder>(*m_pMoveCosts, *m_pSteps, *m_worldMap);
    m_pFirstContact = std::make_unique<FirstContactResolver>(
        *m_pDiplomacy, m_factions);
    m_worldMap->GetUnitPositions().OnUnitMoved.Connect(
        std::bind_front(&GameState::OnUnitMoved_, this));
    // Safe while this is still being built: the executors only store the reference.
    m_pUnitMoves = std::make_unique<UnitMoveExecutor>(*m_pMoveCosts, *m_pSteps, *m_worldMap,
                                                      m_rng, *this);
    m_pUnitOrderExecutor = std::make_unique<UnitOrderExecutor>(
        *m_pUnitMoves, *m_pSteps, *m_worldMap, *m_pTileEffects, *m_pPathfinder,
        *rGameData.terrainOperationRegistry, m_rng, *this);
    m_pUnitAttacks = std::make_unique<UnitAttackExecutor>(
        *m_pMoveCosts, *m_pSteps, *m_worldMap, *m_pTileEffects, *rGameData.moraleCalculator, m_rng,
        *this, *m_pUnitOrderExecutor);
    m_pProbeActions = std::make_unique<ProbeActionExecutor>(
        *rGameData.moraleCalculator, *rGameData.probeActionsConfig, m_rng);
}

GameState::~GameState()
{
    // Factions are partly destroyed by now, so the session list must not be queried.
    if (m_pTileEffects)
    {
        m_pTileEffects->UnbindWorldEffects();
    }
}

const GameDataContext& GameState::GetGameData() const
{
    return m_rGameData;
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

void GameState::IncrementMissionYear()
{
    ++m_missionYear;
}

int GameState::GetYearsSinceFirstPlayableYear() const
{
    return std::max(0, m_missionYear - k_FirstPlayableMissionYear);
}

int GameState::GetTurnNumber() const
{
    return m_missionYear - k_StartingMissionYear;
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
    // Mixes peers' local pool versions, not composed ones, so council extras do not feed back into
    // the stamp.
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
    if (m_pWorldEvents)
    {
        stamp = mix(stamp, m_pWorldEvents->GetRevision());
    }
    return stamp;
}

std::vector<ActiveEffect_t> GameState::CollectSessionWorldEffects() const
{
    std::vector<ActiveEffect_t> result;
    AppendWorldSources_(nullptr, result);
    return result;
}

std::vector<ActiveEffect_t> GameState::CollectWorldExtras(const Faction& rFor) const
{
    std::vector<ActiveEffect_t> result;
    AppendWorldSources_(&rFor, result);
    return result;
}

void GameState::AppendWorldSources_(const Faction* pFor, std::vector<ActiveEffect_t>& rOut) const
{
    for (const auto& pFaction : m_factions)
    {
        if (pFaction.get() == pFor)
        {
            continue;
        }
        // Peer *local* pool only: composed GetActiveEffects would re-emit council extras.
        const std::vector<ActiveEffect_t>& rLocal = pFaction->GetLocalActiveEffects().effects;
        auto worldEffects = FilterByScope(rLocal, EffectScope_t::WorldGlobal);
        rOut.insert(rOut.end(), worldEffects.begin(), worldEffects.end());
    }
    if (m_pCouncil)
    {
        // Council config addresses are stable across rebuilds; the revision bump refreshes caches.
        const std::vector<ActiveEffect_t>& councilWorld = m_pCouncil->CollectWorldEffects();
        rOut.insert(rOut.end(), councilWorld.begin(), councilWorld.end());
        if (pFor != nullptr)
        {
            const std::vector<ActiveEffect_t>& councilFaction =
                m_pCouncil->CollectFactionEffects(*pFor);
            rOut.insert(rOut.end(), councilFaction.begin(), councilFaction.end());
        }
    }
    if (m_pWorldEvents)
    {
        m_pWorldEvents->AppendActiveEffects(rOut);
    }
    AppendActiveEffects(m_rGameData.worldRules, nullptr, "world_rules", rOut);
}

Faction& GameState::AddFaction(std::unique_ptr<Faction> pFaction)
{
    if (!pFaction)
    {
        throw std::invalid_argument("GameState::AddFaction: pFaction is null");
    }
    if (&pFaction->GetWorldMap() != &GetWorldMap())
    {
        throw std::invalid_argument(
            "GameState::AddFaction: faction was built against another WorldMap");
    }
    // In m_factions before AttachToSession_, whose closing sweep iterates Factions().
    m_factions.push_back(std::move(pFaction));
    Faction& rAdded = *m_factions.back();
    AttachToSession_(rAdded);
    return rAdded;
}

void GameState::AttachToSession_(Faction& rFaction)
{
    if (rFaction.GetDefinition().identity.participatesInCouncil)
    {
        m_pCouncil->Seat(rFaction);
    }
    // The session wiring that has to close over this GameState.
    WireFactionObservers_(rFaction);
    WireUnitObservers_(rFaction.GetUnitManager());
    WireBaseObservers_(rFaction);

    // Catches up on bases and units the faction already owns; a no-op for a new faction.
    RebuildTerritory();
    rFaction.RebuildVisibility();
    // Contact is symmetric: an incumbent who already sees the newcomer's bases must learn of it
    // too.
    ConsiderAllObservers_();
}

void GameState::ConsiderAllObservers_()
{
    for (Faction& rObserver : Factions())
    {
        m_pFirstContact->ConsiderObserver(rObserver);
    }
}

// Visibility stays on Faction; the position index owns position only.
void GameState::OnUnitMoved_(Unit& rMoved)
{
    rMoved.GetFaction().RebuildVisibility();
    // Stationary factions whose vision already covers the new tile must also meet.
    m_pFirstContact->ConsiderUnit(rMoved);
}

void GameState::OnImprovementVisited(Unit& rVisitor)
{
    if (rVisitor.GetFaction().IsPlayerControlled())
    {
        EnqueueForPlayer(*this, ImprovementVisitInteraction_t{rVisitor.GetUnitId()});
        return;
    }
    ApplyVisitEffects(*this, rVisitor, m_rng);
}

void GameState::WireFactionObservers_(Faction& rFaction)
{
    rFaction.OnSecretProjectDestroyed.Connect([this](const BuildingId_t& rBuildingId)
    {
        MarkSecretProjectDestroyed(rBuildingId);
    });
    rFaction.GetResearch().OnTechDiscovered.Connect(
        [this, &rFaction](const TechId& rTechId)
        {
            ApplyTechDiscoverEffects(*this, rFaction, rTechId);
            rFaction.EnsureResearchTarget();
        });
    rFaction.OnBaseListChanged.Connect([this]()
    {
        RebuildTerritory();
        // A new base may sit inside another faction's existing vision cone.
        ConsiderAllObservers_();
    });
    rFaction.OnVisibilityRebuilt.Connect([this](Faction& rObserver)
    {
        m_pFirstContact->ConsiderObserver(rObserver);
    });
}

void GameState::WireUnitObservers_(UnitManager& rUnits)
{
    // Drop destroyed units from every faction's contact-reveal set (address reuse safety).
    rUnits.OnUnitDestroyed.Connect([this](Unit& rDestroyed)
    {
        for (Faction& rOther : Factions())
        {
            rOther.GetRevealedUnits().Forget(rDestroyed);
        }
    });
    // Unit-produced triggers before first-contact so XP/grants are applied when peers scan.
    rUnits.OnUnitCreated.Connect(
        [this](Unit& rCreated, BaseManager* pBuiltAt)
        {
            if (pBuiltAt)
            {
                ApplyUnitProducedTriggers(*this, rCreated, *pBuiltAt);
            }
            // Created into another faction's existing vision (no move event).
            m_pFirstContact->ConsiderUnit(rCreated);
        });
    rUnits.OnUnitAdopted.Connect([this](Unit& rAdopted)
    {
        // A transfer is not a birth, but the unit may still be seen by a faction it has not met.
        m_pFirstContact->ConsiderUnit(rAdopted);
    });
}

void GameState::WireBaseObservers_(Faction& rFaction)
{
    rFaction.OnBaseAdded.Connect(std::bind_front(&GameState::WireBaseSession_, this));
    for (BaseManager& rBase : rFaction.Bases())
    {
        WireBaseSession_(rBase);
    }
}

void GameState::WireBaseSession_(BaseManager& rBase)
{
    if (!m_sessionWiredBases.insert(&rBase).second)
    {
        return;
    }
    // on_complete effects before any later UI/mod observers of the same signal.
    rBase.OnProductionCompleted.Connect(
        [this](const ProductionCompleted_t& rCompleted)
        {
            ApplyProductionCompleteEffects_(*this, rCompleted);
        });
    WireMoodNotices_(*this, rBase);
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

Faction& GameState::RequireFaction(FactionId_t factionId)
{
    return const_cast<Faction&>(static_cast<const GameState*>(this)->RequireFaction(factionId));
}

const Faction& GameState::RequireFaction(FactionId_t factionId) const
{
    const Faction* pFaction = FindFaction(factionId);
    if (!pFaction)
    {
        throw std::invalid_argument("GameState: no faction with id " + std::to_string(factionId));
    }
    return *pFaction;
}

const Faction& GameState::GetPlayerFaction() const
{
    for (const auto& pFaction : m_factions)
    {
        if (pFaction->IsPlayerControlled())
        {
            return *pFaction;
        }
    }
    throw std::logic_error("GameState: no faction is player-controlled");
}

Faction& GameState::GetPlayerFaction()
{
    const GameState& rConstThis = *this;
    return const_cast<Faction&>(rConstThis.GetPlayerFaction());
}

DiplomacyLedger& GameState::GetDiplomacyLedger()
{
    return *m_pDiplomacy;
}

const DiplomacyLedger& GameState::GetDiplomacyLedger() const
{
    return *m_pDiplomacy;
}

AtrocityLedger& GameState::GetAtrocityLedger()
{
    return *m_pAtrocities;
}

const AtrocityLedger& GameState::GetAtrocityLedger() const
{
    return *m_pAtrocities;
}

MindControlLedger& GameState::GetMindControlLedger()
{
    return *m_pMindControl;
}

const MindControlLedger& GameState::GetMindControlLedger() const
{
    return *m_pMindControl;
}

EcologyLedger& GameState::GetEcologyLedger()
{
    return *m_pEcology;
}

const EcologyLedger& GameState::GetEcologyLedger() const
{
    return *m_pEcology;
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
    Unit& rAttacker, Unit& rDefender, std::mt19937& rRng)
{
    return ac::TryInterceptAttack(*this, rAttacker, rDefender, rRng);
}

BaseConquestResult_t GameState::ResolvePostCombatBaseConquest(
    Unit& rAttacker, const Tile& rDefenderTile, std::mt19937& rRng)
{
    return ac::ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, *this, rRng);
}

BaseConquestResult_t GameState::ResolveBaseEntryConquest(Unit& rMover, std::mt19937& rRng)
{
    return ac::ResolveBaseEntryConquest(rMover, *this, rRng);
}

void GameState::OnHostileAct(const HostileAct_t& rAct)
{
    ApplyHostileAct(*this, rAct);
}

void GameState::OnTerritoryEntryRefused(Unit& rUnit, FactionId_t territoryOwner)
{
    EnqueueForPlayer(*this, TerritoryEntryInteraction_t{rUnit.GetUnitId(), territoryOwner});
}

bool GameState::ApplyTerraformCompleteEffects(Tile& rTile, const TerraformProject_t& rProject,
                                              Unit& rFormer, std::mt19937& rRng)
{
    TriggeredEffectContext_t context(*this, rFormer.GetFaction());
    context.pUnit = &rFormer;
    context.pTile = &rTile;
    context.pRng = &rRng;
    const std::vector<TriggeredEffectResult_t> results =
        ApplyTriggeredEffects(rProject.onCompleteEffects, context);
    return !results.empty();
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

UnitMoveExecutor& GameState::GetUnitMoves()
{
    return *m_pUnitMoves;
}

UnitOrderExecutor& GameState::GetUnitOrderExecutor()
{
    return *m_pUnitOrderExecutor;
}

UnitAttackExecutor& GameState::GetUnitAttacks()
{
    return *m_pUnitAttacks;
}

void GameState::ConsiderHoldLink(Unit& rUnit)
{
    if (!UnitHasHoldLink(rUnit))
    {
        return;
    }
    if (rUnit.GetFaction().IsPlayerControlled())
    {
        EnqueueForPlayer(*this, ArtifactLinkInteraction_t{rUnit.GetUnitId()});
        return;
    }
    ApplyHoldLink(*this, rUnit);
}

ProbeActionExecutor& GameState::GetProbeActions()
{
    return *m_pProbeActions;
}

const ProbeActionExecutor& GameState::GetProbeActions() const
{
    return *m_pProbeActions;
}

void GameState::CreateWorldEvents()
{
    if (m_pWorldEvents)
    {
        throw std::logic_error("GameState::CreateWorldEvents: world events already exist");
    }
    m_pWorldEvents = std::make_unique<WorldEventTracker>(*m_rGameData.worldEventsConfig);
}

WorldEventTracker* GameState::GetWorldEvents()
{
    return m_pWorldEvents.get();
}

const WorldEventTracker* GameState::GetWorldEvents() const
{
    return m_pWorldEvents.get();
}

CouncilVoteExecutor& GameState::GetCouncilVotes()
{
    return *m_pCouncilVotes;
}

PlanetaryCouncil& GameState::GetPlanetaryCouncil()
{
    return *m_pCouncil;
}

const PlanetaryCouncil& GameState::GetPlanetaryCouncil() const
{
    return *m_pCouncil;
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

void GameState::ReapRazedBases()
{
    for (Faction& rFaction : Factions())
    {
        rFaction.ReapRazedBases();
    }
}

void GameState::MarkSecretProjectDestroyed(const std::string& buildingId)
{
    m_destroyedSecretProjects.insert(buildingId);
}

bool GameState::IsSecretProjectDestroyed(const std::string& buildingId) const
{
    return m_destroyedSecretProjects.contains(buildingId);
}

void GameState::OnVisibilitySettingsChanged_()
{
    for (Faction& rFaction : Factions())
    {
        rFaction.RebuildVisibility();
    }
}

} // namespace ac
