#include "game/Engine.h"
#include "game/GameState.h"
#include "game/CurrentGameState.h"
#include "game/council/CouncilAiStub.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/GameSettings.h"
#include "game/DifficultyConfig.h"
#include "game/natives/NativeLifeLevelConfig.h"
#include "game/world-events/WorldEventConfig.h"
#include "game/faction/Faction.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/economy/EconomyManager.h"
#include "game/stages/TurnStageFactory.h"
#include "game/stages/TurnProcessor.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "input/KeyMapping.h"
#include "lib/EventBus.h"
#include "game/EventBridge.h"
#include "lib/GameEvent.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/GameDataContext.h"
#include <random>
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/effects/TileEffectsContext.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/faction/FactionRegistry.h"
#include "game/faction/FactionConfig.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/population/PopContainer.h"
#include "game/units/Military.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/combat/MoraleConfig.h"
#include "game/base/FoundBaseRules.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainOperationRegistry.h"
#include "ui/HotkeyConfig.h"
#include "ui/IGameView.h"
#include "game/worldgen/MapGenerationConfig.h"
#include "game/worldgen/WorldGenPresetRegistry.h"
#include "game/worldgen/WorldGenerator.h"
#include "game/worldgen/WorldGenPresetConfigParser.h"
#include "ui/SpriteLibrary.h"
#include "ui/MapRenderer.h"
#include "ui/UIManager.h"
#include "ui/ViewFactory.h"
#include "ui/InteractionPresenter.h"
#include "ui/WaterShading.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

// #define AC_PLACE_TEST_IMPROVEMENTS 0

namespace ac
{

namespace
{

Tile* PickStartingBaseTile_(WorldMap& rMap,
                            const std::vector<const BaseManager*>& rPlacedBases,
                            const std::vector<std::pair<int, int>>& rPreferred)
{
    const auto isValidStart = [&](const Tile& rTile) {
        if (!rTile.IsLand())
        {
            return false;
        }
        if (rMap.GetWorkedTiles().IsWorked(rTile))
        {
            return false;
        }
        return !IsTooCloseToAnyBase(rTile, rPlacedBases);
    };

    for (const auto& [x, y] : rPreferred)
    {
        if (Tile* pTile = rMap.GetTile(x, y); pTile && isValidStart(*pTile))
        {
            return pTile;
        }
    }

    for (const auto& pTilePtr : rMap.GetTiles())
    {
        Tile* pTile = pTilePtr.get();
        if (pTile && isValidStart(*pTile))
        {
            return pTile;
        }
    }

    return nullptr;
}

// Closest free land or water tile to rNear that respects founding separation.
Tile* FindPreviewBaseTile_(WorldMap& rMap, const Tile& rNear, bool bWantWater,
                           const std::vector<const BaseManager*>& rPlacedBases)
{
    Tile* pBest = nullptr;
    int bestDistance = std::numeric_limits<int>::max();
    const int mapWidth = rMap.GetWidth();
    for (const auto& pTilePtr : rMap.GetTiles())
    {
        Tile* pTile = pTilePtr.get();
        if (!pTile || (bWantWater ? !pTile->IsWater() : !pTile->IsLand()))
        {
            continue;
        }
        if (rMap.GetWorkedTiles().IsWorked(*pTile)
            || IsTooCloseToAnyBase(*pTile, rPlacedBases))
        {
            continue;
        }
        const int distance = ChebyshevDistance(rNear, *pTile, mapWidth);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            pBest = pTile;
        }
    }
    return pBest;
}

// Temporary: extra player bases to eyeball size stages, water bases, and defense overlays.
void PlacePreviewFactionBases_(GameState& rGameState, Faction& rPlayer,
                               std::vector<const BaseManager*>& rPlacedBases)
{
    WorldMap& rMap = rGameState.GetWorldMap();
    const Tile& rHq = rPlayer.Bases().front().GetTile();

    struct PreviewBase_t
    {
        const char* name;
        int population;
        bool bWater;
        bool bPerimeter;
        bool bTachyon;
    };
    // Pops hit SMAC size stages 1–4 (1–3 / 4–7 / 8–14 / 15+); oversized pops are demo-only.
    const PreviewBase_t k_Previews[] = {
        {"Preview Outpost", 1, false, false, false},
        {"Preview Fortress", 5, false, true, false},
        {"Preview Citadel", 10, false, true, true},
        {"Preview Metropolis", 16, false, true, true},
        {"Preview Shoal", 2, true, false, false},
        {"Preview Harbor", 6, true, true, false},
        {"Preview Seawall", 12, true, true, true},
    };

    for (const PreviewBase_t& rPreview : k_Previews)
    {
        Tile* pTile = FindPreviewBaseTile_(rMap, rHq, rPreview.bWater, rPlacedBases);
        if (!pTile)
        {
            std::cout << "Engine setup: skip preview base '" << rPreview.name
                      << "' (no free " << (rPreview.bWater ? "water" : "land") << " tile)\n";
            continue;
        }
        BaseManager* pBase = rPlayer.CreateBase(
            rGameState.AllocateBaseId(), rPreview.name, pTile, rGameState.GetTileEffects(),
            rPreview.population);
        if (!pBase)
        {
            continue;
        }
        rPlacedBases.push_back(pBase);
        if (rPreview.bPerimeter)
        {
            pBase->GetBuildingManager().AddBuilding("Perimeter_Defense");
        }
        if (rPreview.bTachyon)
        {
            pBase->GetBuildingManager().AddBuilding("Tachyon_Field");
        }
    }
}

// One starting base per faction. The player prefers x=0 so wrap is easy to exercise; slots keep
// k_MinBaseFoundingSeparation and are never reused.
std::vector<std::pair<int, int>> PreferredStartPositions_(const WorldMap& rMap)
{
    const int centerY = rMap.GetHeight() / 2;
    const int mapWidth = rMap.GetWidth();
    std::vector<std::pair<int, int>> positions;
    for (int x = (centerY & 1); x < mapWidth; x += 10)
    {
        positions.push_back({x, centerY});
    }
    return positions;
}

void LogPopChange_(const GameEvent& rEvent)
{
    if (const auto* pGained = std::get_if<EvBaseGainedPop>(&rEvent))
    {
        std::cout << "[EVENT] Base " << pGained->baseId << " (Faction " << pGained->factionId
                  << ") gained a pop! New size: " << pGained->newSize << "\n";
    }
    else if (const auto* pLost = std::get_if<EvBaseLostPop>(&rEvent))
    {
        std::cout << "[EVENT] Base " << pLost->baseId << " (Faction " << pLost->factionId
                  << ") lost a pop! New size: " << pLost->newSize << "\n";
    }
}

#ifdef AC_PLACE_TEST_IMPROVEMENTS
// Temporary test setup: one of each buildable improvement around every starting base, to eyeball
// sprites, coexistence and auras.

// The nearest tile to rBaseTile that is not a base and passes rAccepts. Throws naming rWhat when
// none does.
Tile& FindNearestHost_(const Tile& rBaseTile, WorldMap& rMap,
                       const std::function<bool(const Tile&)>& rAccepts, const std::string& rWhat)
{
    const int searchRadius = std::max(rMap.GetWidth(), rMap.GetHeight());
    Tile* pHost = nullptr;
    int bestDistance = searchRadius + 1;
    ForEachNeighborInChebyshevRadius(
        rBaseTile, rMap, searchRadius,
        [&](Tile* pTile, int distance) {
            if (pTile && distance < bestDistance && !pTile->HasImprovement(ImprovementIds::k_Base)
                && rAccepts(*pTile))
            {
                pHost = pTile;
                bestDistance = distance;
            }
        });
    if (!pHost)
    {
        throw std::runtime_error("Engine setup: no tile near base for " + rWhat);
    }
    return *pHost;
}

void PlaceOneOfEachImprovement_(const Tile& rBaseTile, WorldMap& rMap,
                                TileEffectsContext& rTileEffects,
                                const ImprovementRegistry& rImprovements, FactionId_t factionId)
{
    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        if (rConfig.placement != OccupantPlacement_t::Improvement
            || rConfig.id == ImprovementIds::k_Base)
        {
            continue;
        }
        Tile& rHost = FindNearestHost_(
            rBaseTile, rMap,
            [&rConfig](const Tile& rTile) { return CanBuildImprovement(rTile, rConfig, {}, {}); },
            "improvement '" + rConfig.id + "'");
        rTileEffects.AddOccupantWithEffects(rHost, rConfig.id);
        std::cout << "Placed " << rConfig.id << " for faction " << factionId << " at ("
                  << rHost.GetX() << ", " << rHost.GetY() << ")\n";
    }
}

// Extra Farms keyed by moisture so arid/moist/wet ground sprites are all visible.
void PlaceMoistureFarms_(const Tile& rBaseTile, WorldMap& rMap, TileEffectsContext& rTileEffects,
                         const ImprovementRegistry& rImprovements, FactionId_t factionId)
{
    const ImprovementConfig_t& rFarm = rImprovements.Get(std::string(ImprovementIds::k_Farm));
    for (const Moisture_t moisture : {Moisture_t::Arid, Moisture_t::Moist, Moisture_t::Wet})
    {
        Tile& rHost = FindNearestHost_(
            rBaseTile, rMap,
            [&rFarm, moisture](const Tile& rTile) {
                return !rTile.HasImprovement(ImprovementIds::k_Farm)
                       && rTile.GetMoisture() == moisture
                       && CanBuildImprovement(rTile, rFarm, {}, {});
            },
            ToString(moisture) + " Farm");
        rTileEffects.AddOccupantWithEffects(rHost, rFarm.id);
        std::cout << "Placed Farm (" << ToString(moisture) << ") for faction " << factionId
                  << " at (" << rHost.GetX() << ", " << rHost.GetY() << ")\n";
    }
}

void PlaceTestImprovementsAroundBases_(WorldMap& rMap, TileEffectsContext& rTileEffects,
                                       const ImprovementRegistry& rImprovements,
                                       GameState& rGameState)
{
    for (Faction& rFaction : rGameState.Factions())
    {
        // One base per faction is enough for the temporary showcase.
        for (BaseManager& rBase : rFaction.Bases())
        {
            PlaceOneOfEachImprovement_(rBase.GetTile(), rMap, rTileEffects, rImprovements,
                                       rFaction.GetFactionId());
            PlaceMoistureFarms_(rBase.GetTile(), rMap, rTileEffects, rImprovements,
                                rFaction.GetFactionId());
            break;
        }
    }

    for (Faction& rFaction : rGameState.Factions())
    {
        rFaction.RebuildVisibility();
    }
}
#endif // AC_PLACE_TEST_IMPROVEMENTS

} // namespace

Engine::Engine()
    : m_pSettings(std::make_unique<GameSettings>())
{
    // Settings first: the window this opens is sized, titled and fonted from them.
    m_pSettings->Load(GameSettings::k_DefaultPath);

    m_pGraphics = CreateGraphics(m_platformEvents, m_pSettings->GetGraphics());
    m_pInput = CreateInput(m_platformEvents);
    if (!m_pGraphics)
    {
        throw std::runtime_error("Failed to create graphics backend");
    }
    m_pSprites = std::make_unique<SpriteLibrary>(
        *m_pGraphics,
        [](const std::string& rPath) { return std::filesystem::exists(rPath); });
    if (!m_pInput)
    {
        throw std::runtime_error("Failed to create input backend");
    }
    m_uiManager = std::make_unique<UIManager>(*m_pGraphics, *m_pInput);
}

Engine::~Engine() = default;

void Engine::Run()
{
    Initialize_();
    PrintWelcome_();

    std::cout << "Graphics backend initialized successfully.\n";
    std::cout << "Starting game loop...\n";

    GameLoop_();

    std::cout << "Exiting game.\n";
}

void Engine::GameLoop_()
{
    while (!m_uiManager->ShouldExit())
    {
        // Before anything reads input, and separate from Render.
        m_pGraphics->PumpEvents();
        if (m_platformEvents.TakeCloseRequest())
        {
            // The same exit flag as every other quit path.
            // TODO: SMAC prompts to save before quitting.
            m_uiManager->RequestExit();
            continue;
        }

        m_uiManager->ProcessInput();
        // Between input and paint, so a UI-queued turn advance never runs from Render.
        m_uiManager->Update();
        if (m_interactionPresenter)
        {
            m_interactionPresenter->Update();
        }
        m_uiManager->Render();
    }
}

void Engine::ProcessTurn_()
{
    // Turn processing can destroy pops and base state that open popups hold. An open modal is
    // ordinary UI traffic, so the turn just waits.
    if (!m_uiManager->CanAdvanceTurn())
    {
        return;
    }

    // Runs until a stage yields for interaction; turn boundaries are handled inside stages.
    m_turnProcessor->Advance(*m_pGameState);
    m_uiManager->MarkFrameDirty();
}

void Engine::Initialize_()
{
    std::cout << "Initializing game engine...\n";

    // App data outlives sessions; the session is what a new game builds; the UI binds to the
    // session. Tests build a session without graphics.
    InitializeApp_();
    StartNewGame_();
    InitializeUi_();
}

void Engine::InitializeApp_()
{
    // Settings are already loaded: the constructor needs them to open the window.
    UiStyle::Load("config/ui/style.json");

    // Every parser plus cross-config id validation; complete or throws.
    m_gameDataContext = std::make_unique<GameDataContext>(LoadGameData(GameDataPaths{}));
    ValidateTerrainArtReferences(Style().tileRenderer.waterShading,
                                 *m_gameDataContext->improvementRegistry);

    // Here, so the message can still point at the settings file.
    m_gameDataContext->difficultyConfig->RequireForSession(
        m_pSettings->GetGameRules().difficultyId);
    m_gameDataContext->nativeLifeLevelConfig->RequireForSession(
        m_pSettings->GetGameRules().nativeLifeLevelId);
}

void Engine::StartNewGame_()
{
    PickSessionSeed_();
    GenerateWorld_();
    m_eventBridge = std::make_unique<EventBridge>(m_pGameState->GetEventBus());
    m_pGameState->GetEventBus().Subscribe(LogPopChange_);

    std::vector<const BaseManager*> placedBases = AddFactions_();
    // Temporary: extra player bases to eyeball land/water sprites and defense overlays.
    if (Faction& rPlayer = m_pGameState->GetPlayerFaction(); !rPlayer.Bases().empty())
    {
        PlacePreviewFactionBases_(*m_pGameState, rPlayer, placedBases);
    }
    // Each AddFaction folded in its own territory; this is the whole-world pass.
    m_pGameState->RebuildTerritory();
    SetUpStartingDiplomacy_();

    m_pGameState->CreateWorldEvents();
    m_councilAiVoteConn = m_pGameState->GetCouncilVotes().OnProposalOpened.ConnectScoped(
        [this](Faction& /*rProposer*/, const std::string& /*rProposalId*/) {
            CastStubCouncilVotes(m_pGameState->GetCouncilVotes(),
                                 m_pGameState->GetPlanetaryCouncil());
        });

#ifdef AC_PLACE_TEST_IMPROVEMENTS
    PlaceTestImprovementsAroundBases_(
        m_pGameState->GetWorldMap(), m_pGameState->GetTileEffects(),
        *m_gameDataContext->improvementRegistry, *m_pGameState);
#endif

    std::cout << "Test setup complete. " << m_pGameState->GetNumFactions() << " faction(s), "
              << m_pGameState->GetPlayerFaction().GetBaseCount() << " base(s)\n";
}

void Engine::PickSessionSeed_()
{
    // One session seed (`seed == 0` picks one); every per-faction random choice derives from it.
    // TODO: save the resolved seed once saves exist, outside GameSettings.
    const MapGenerationConfig_t& rWorldConfig = m_pSettings->GetMapGeneration();
    m_sessionSeed = rWorldConfig.seed != 0 ? rWorldConfig.seed : std::random_device{}();
    std::cout << "Session seed: " << m_sessionSeed << "\n";
}

void Engine::GenerateWorld_()
{
    const MapGenerationConfig_t& rWorldConfig = m_pSettings->GetMapGeneration();
    const WorldGenPresetConfig_t& rPreset =
        m_gameDataContext->worldGenPresetRegistry->Get(rWorldConfig.presetId);
    WorldGenerator worldGen;
    std::unique_ptr<WorldMap> pWorld =
        worldGen.Generate(rWorldConfig, rPreset, *m_gameDataContext->worldGenDecorationConfig,
                          m_gameDataContext->worldGenLandmarks,
                          *m_gameDataContext->improvementRegistry,
                          m_gameDataContext->elevationRules,
                          m_sessionSeed);
    // Separate from world generation, so map size does not shift combat rolls.
    const auto combatSeed = static_cast<uint32_t>(m_sessionSeed ^ 0x5BF03635u);
    m_pGameState = std::make_unique<GameState>(std::move(pWorld), *m_gameDataContext,
                                               *m_pSettings, combatSeed);
    m_pGameStateScope = std::make_unique<GameStateScope>(*m_pGameState);

    const WorldMap& rMap = m_pGameState->GetWorldMap();
    std::cout << "Generated world map: " << rMap.GetWidth() << "x" << rMap.GetHeight() << "\n";
}

std::vector<const BaseManager*> Engine::AddFactions_()
{
    const std::vector<std::pair<int, int>> preferredStartPositions =
        PreferredStartPositions_(m_pGameState->GetWorldMap());
    std::vector<const BaseManager*> placedBases;
    size_t positionIndex = 0;
    for (const FactionConfig_t& rFactionConfig : m_gameDataContext->factionRegistry->GetAll())
    {
        AddFaction_(rFactionConfig, positionIndex, preferredStartPositions, placedBases);
        ++positionIndex;
    }
    return placedBases;
}

void Engine::AddFaction_(const FactionConfig_t& rFactionConfig, size_t positionIndex,
                         const std::vector<std::pair<int, int>>& rPreferredStartPositions,
                         std::vector<const BaseManager*>& rPlacedBases)
{
    // Single-player for now: the first faction is the human player.
    const bool bNativeLife = IsNativeLifeFaction(rFactionConfig.identity.species);
    const bool bIsPlayerControlled = (positionIndex == 0) && !bNativeLife;
    // Per faction, so adding a faction does not shift another's picks.
    const auto factionSeed =
        static_cast<uint32_t>(m_sessionSeed + 0x9E3779B9u * (positionIndex + 1));
    auto pFaction = std::make_unique<Faction>(
        m_pGameState->AllocateFactionId(),
        bIsPlayerControlled,
        rFactionConfig,
        *m_gameDataContext,
        m_pGameState->GetWorldMap(),
        *m_pSettings,
        factionSeed);

    // Every base the faction ever gains (founding, capture, trade) reaches the bridge through
    // OnBaseAdded.
    pFaction->OnBaseAdded.Connect([this](BaseManager& rBase) { m_eventBridge->WireBase(rBase); });

    if (bNativeLife)
    {
        Faction& rFaction = m_pGameState->AddFaction(std::move(pFaction));
        m_eventBridge->WireFaction(rFaction);
        return;
    }

    Tile* pStartTile = PickStartingBaseTile_(
        m_pGameState->GetWorldMap(), rPlacedBases, rPreferredStartPositions);
    if (!pStartTile)
    {
        throw std::runtime_error(
            "Engine setup: no valid land tile for faction '" + rFactionConfig.id
            + "' starting base");
    }

    BaseManager* pBase = pFaction->CreateBase(
        m_pGameState->AllocateBaseId(), pFaction->SuggestBaseName(), pStartTile,
        m_pGameState->GetTileEffects(),
        /*initialPopulation=*/3);
    rPlacedBases.push_back(pBase);

    Faction& rFaction = m_pGameState->AddFaction(std::move(pFaction));
    // After AddFaction, once the faction's id is final.
    m_eventBridge->WireFaction(rFaction);
    rFaction.GetEconomy().AddEnergy(10);
    AddTestUnits_(rFaction, *pStartTile, *pBase);
}

// Temporary test units so fog-of-war vision and specials are easy to exercise.
void Engine::AddTestUnits_(Faction& rFaction, const Tile& rStart, BaseManager& rBase)
{
    const UnitDesign& rScout =
        AddTestDesign_(rFaction, "HoverTank", "Missile_Weapons", "basic scout");
    const UnitDesign& rProbe = AddTestDesign_(rFaction, "Infantry", "Probe_Team", "probe team");
    if (!rFaction.IsPlayerControlled())
    {
        // Enemy scout / probe beside the AI base for multi-faction checks.
        PlaceTestUnit_(rFaction, rScout, rStart, 1, 0, rBase);
        PlaceTestUnit_(rFaction, rProbe, rStart, 1, -1, rBase);
        return;
    }

    const UnitDesign& rColony = AddTestDesign_(rFaction, "Infantry", "Colony_Pod", "colony pod");
    const UnitDesign& rCrawler =
        AddTestDesign_(rFaction, "Infantry", "Supply_Crawler", "supply crawler");
    const UnitDesign& rNeedlejet =
        AddTestDesign_(rFaction, "Needlejet", "Missile_Weapons", "needlejet");
    const UnitDesign& rMissile = AddTestDesign_(rFaction, "Missile", "Missile_Weapons", "missile");

    // Vision-1 HoverTank scout beside the base (Deep Radar would stack to 2).
    PlaceTestUnit_(rFaction, rScout, rStart, 1, 0, rBase);
    PlaceTestUnit_(rFaction, rColony, rStart, 1, 1, rBase);
    PlaceTestUnit_(rFaction, rCrawler, rStart, 2, 1, rBase);
    PlaceTestUnit_(rFaction, rProbe, rStart, 1, -1, rBase);
    PlaceTestUnit_(rFaction, rNeedlejet, rStart, 0, 0, rBase);
    PlaceTestUnit_(rFaction, rMissile, rStart, 0, 0, rBase);
}

// Unarmoured, fission-powered: only the chassis and weapon vary between test designs.
const UnitDesign& Engine::AddTestDesign_(Faction& rFaction, const std::string& rChassisId,
                                         const std::string& rWeaponId, const std::string& rLabel)
{
    const UnitComponentRegistry& rComponents = *m_gameDataContext->unitComponentRegistry;
    const std::unordered_map<std::string, const UnitComponentConfig_t*> parts = {
        {"chassis", &rComponents.Get(rChassisId)},
        {"weapon",  &rComponents.Get(rWeaponId)},
        {"armour",  &rComponents.Get("No_Armour")},
        {"reactor", &rComponents.Get("Fission_Plant")},
    };
    const std::vector<UnitSlotConfig_t>& rSlots = m_gameDataContext->unitSlotRegistry->GetAll();
    auto pDesign = std::make_unique<UnitDesign>(rSlots, parts);
    const UnitDesign& rDesign = *pDesign;
    if (!rFaction.GetMilitary().AddDesign(std::move(pDesign)))
    {
        throw std::runtime_error("Engine setup: failed to add " + rLabel + " design");
    }
    return rDesign;
}

void Engine::PlaceTestUnit_(Faction& rFaction, const UnitDesign& rDesign, const Tile& rStart,
                            int p, int q, BaseManager& rBase)
{
    WorldMap& rMap = m_pGameState->GetWorldMap();
    const Tile* pTile = GetTileAtLatticeOffset(rMap, rStart, p, q);
    rFaction.GetUnitManager().CreateUnit(
        m_pGameState->AllocateUnitId(), rDesign, rMap.GetUnitPositions(), *pTile,
        UnitOrigin::Issued(rBase));
}

// Temporary: give the player commerce partners — first AI Pact, second AI Treaty.
void Engine::SetUpStartingDiplomacy_()
{
    DiplomacyLedger& rDiplomacy = m_pGameState->GetDiplomacyLedger();
    const FactionId_t playerId = m_pGameState->GetPlayerFaction().GetFactionId();
    int aiIndex = 0;
    for (Faction& rFaction : m_pGameState->Factions())
    {
        if (rFaction.GetFactionId() == playerId
            || IsNativeLifeFaction(rFaction.GetDefinition().identity.species))
        {
            continue;
        }

        rDiplomacy.SetKnown(playerId, rFaction.GetFactionId(), true);
        if (aiIndex == 0)
        {
            ApplyStatusChange(*m_pGameState, playerId, rFaction.GetFactionId(),
                              DiplomaticStatus_t::Pact);
        }
        else if (aiIndex == 1)
        {
            ApplyStatusChange(*m_pGameState, playerId, rFaction.GetFactionId(),
                              DiplomaticStatus_t::Treaty);
        }
        ++aiIndex;
    }
}

void Engine::InitializeUi_()
{
    CreateTurnProcessor_();
    m_pHotkeys = std::make_unique<HotkeyConfig>(HotkeyConfig::Load(
        m_gameDataContext->paths.hotkeys,
        *m_gameDataContext->improvementRegistry,
        *m_gameDataContext->terrainOperationRegistry));
    m_pMapRenderer = std::make_unique<MapRenderer>(*m_pSprites, *m_pGameState, Style().tileRenderer,
                                                   Style().mapRenderer);
    m_viewFactory = std::make_unique<ViewFactory>(
        *m_pGameState,
        *m_gameDataContext,
        *m_pHotkeys,
        *m_pGraphics,
        *m_pMapRenderer,
        *m_pSprites,
        *m_pSettings);

    const WindowLayout_t fullscreen = m_viewFactory->GetFullscreenLayout();
    auto pWorldView = m_viewFactory->CreateWorldView(
        fullscreen,
        WorldView::Callbacks_t{
            .onProcessTurn = [this]() { ProcessTurn_(); },
            .onRequestExit = [this]() { m_uiManager->RequestExit(); },
            .onOpenBase = [this](BaseManager& rBase)
            { m_uiManager->PushView(m_viewFactory->CreateBaseView(rBase)); },
            .onOpenCombat = std::bind_front(&Engine::PushCombatView_, this),
            .onOpenCommlinks = std::bind_front(&Engine::PushCommlinksView_, this),
        });
    WorldView& rWorldView = *pWorldView;
    BindViewShortcuts_(fullscreen);
    m_uiManager->SetWorldView(std::move(pWorldView));

    m_interactionPresenter = std::make_unique<InteractionPresenter>(
        *m_pGameState,
        *m_uiManager,
        *m_viewFactory,
        rWorldView,
        [this]() { ProcessTurn_(); });

    // Start processing until the first interactive yield.
    m_turnProcessor->Advance(*m_pGameState);
}

void Engine::CreateTurnProcessor_()
{
    m_turnStageFactory = std::make_unique<TurnStageFactory>();
    m_turnStageFactory->LoadConfig("config/turn_stages.json");
    auto registries = m_turnStageFactory->CreateStages();
    std::vector<std::string> stageOrder;
    for (const auto& config : m_turnStageFactory->GetStageConfigs())
    {
        stageOrder.push_back(config.id);
    }
    m_turnProcessor = std::make_unique<TurnProcessor>(
        std::move(registries.global), std::move(registries.perFaction), std::move(stageOrder));
}

void Engine::PushCombatView_(CombatResult_t result,
                             const Tile& rAttackerTile,
                             const Tile& rDefenderTile,
                             std::string attackerName,
                             std::string defenderName,
                             WorldDisplay& rWorldDisplay,
                             WindowLayout_t mapLayout,
                             std::function<void()> onFinished)
{
    const WindowLayout_t fullscreen = m_viewFactory->GetFullscreenLayout();
    auto pCombatView = m_viewFactory->CreateCombatView(
        fullscreen,
        std::move(result),
        rAttackerTile,
        rDefenderTile,
        std::move(attackerName),
        std::move(defenderName),
        rWorldDisplay,
        mapLayout,
        std::move(onFinished));
    m_uiManager->PushView(std::move(pCombatView));
}

void Engine::PushCommlinksView_()
{
    const WindowLayout_t fullscreen = m_viewFactory->GetFullscreenLayout();
    auto pCommlinks = m_viewFactory->CreateCommlinksView(
        fullscreen,
        [this, fullscreen]() {
            m_uiManager->PushView(m_viewFactory->CreateCouncilVoteView(fullscreen));
        });
    m_uiManager->PushView(std::move(pCommlinks));
}

void Engine::BindViewShortcuts_(const WindowLayout_t& rFullscreen)
{
    const auto bindView = [&](HotkeyAction_t action, auto createView)
    {
        if (const std::optional<HotkeyChord_t> chord = m_pHotkeys->Find(action))
        {
            m_uiManager->RegisterViewShortcut(*chord, createView);
        }
    };
    const WindowLayout_t fullscreen = rFullscreen;
    bindView(HotkeyAction_t::Research, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateResearchView(fullscreen);
    });
    bindView(HotkeyAction_t::SocialEngineering, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateSocialEngineeringView(fullscreen);
    });
    bindView(HotkeyAction_t::UnitDesigner, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateUnitDesignerView(fullscreen);
    });
    bindView(HotkeyAction_t::Settings, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateSettingsView(fullscreen);
    });
    bindView(HotkeyAction_t::Satellites, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateSatelliteView(fullscreen);
    });
}

void Engine::PrintWelcome_() const
{
    std::cout << "Welcome to Alpha Centauri (C++ rebuild)!\n";
}

} // namespace ac
