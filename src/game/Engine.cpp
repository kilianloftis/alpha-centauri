#include "game/Engine.h"
#include "game/GameState.h"
#include "game/council/CouncilAiStub.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/GameSettings.h"
#include "game/DifficultyConfig.h"
#include "game/NativeLifeLevelConfig.h"
#include "game/world-events/WorldEventConfig.h"
#include "game/Faction.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticTransitionEffects.h"
#include "game/faction/EconomyManager.h"
#include "game/TurnStageFactory.h"
#include "game/TurnProcessor.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "input/KeyMapping.h"
#include "lib/EventBus.h"
#include "game/EventBridge.h"
#include "lib/GameEvent.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
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
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/faction/Military.h"
#include "game/faction/UnitManager.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/MoraleConfig.h"
#include "game/units/FoundBaseRules.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainOperationRegistry.h"
#include "ui/HotkeyConfig.h"
#include "ui/IGameView.h"
#include "game/map/MapGenerationConfig.h"
#include "game/map/WorldGenPresetRegistry.h"
#include "game/map/WorldGenerator.h"
#include "game/map/WorldGenPresetConfigParser.h"
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

#define AC_PLACE_TEST_IMPROVEMENTS 0

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
        return !IsTooCloseToAnyBase(rTile, rMap, rPlacedBases);
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
            || IsTooCloseToAnyBase(*pTile, rMap, rPlacedBases))
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
            rGameState.GetSecretProjectAvailability(), rPreview.population, rPreview.bWater);
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

#ifdef AC_PLACE_TEST_IMPROVEMENTS
// Temporary test setup: one of each buildable improvement around every starting base so
// sprites, coexistence, and tile-effect auras are easy to eyeball in a new game.
void PlaceTestImprovementsAroundBases_(WorldMap& rMap, TileEffectsContext& rTileEffects,
                                       const ImprovementRegistry& rImprovements,
                                       GameState& rGameState)
{
    const int searchRadius =
        std::max(rMap.GetWidth(), rMap.GetHeight());

    for (Faction& rFaction : rGameState.Factions())
    {
        for (BaseManager& rBase : rFaction.Bases())
        {
            const Tile& rBaseTile = rBase.GetTile();
            for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
            {
                if (rConfig.placement != OccupantPlacement_t::Improvement
                    || rConfig.id == ImprovementIds::k_Base)
                {
                    continue;
                }

                Tile* pHost = nullptr;
                int bestDistance = searchRadius + 1;
                ForEachTileInChebyshevRadius(
                    rBaseTile, rMap, searchRadius, /*includeOrigin=*/false,
                    [&](Tile* pTile, int distance) {
                        if (!pTile || pTile->HasImprovement(ImprovementIds::k_Base)
                            || !CanBuildImprovement(*pTile, rConfig) || distance >= bestDistance)
                        {
                            return;
                        }
                        pHost = pTile;
                        bestDistance = distance;
                    });

                if (!pHost)
                {
                    throw std::runtime_error(
                        "Engine setup: no tile near base for improvement '" + rConfig.id + "'");
                }

                rTileEffects.AddOccupantWithEffects(*pHost, rConfig.id);
                std::cout << "Placed " << rConfig.id << " for faction " << rFaction.GetFactionId()
                          << " at (" << pHost->GetX() << ", " << pHost->GetY() << ")\n";
            }

            // Extra Farms keyed by moisture so arid/moist/wet ground sprites are all visible.
            const ImprovementConfig_t& rFarm = rImprovements.Get(std::string(ImprovementIds::k_Farm));
            for (const Moisture_t moisture :
                 {Moisture_t::Arid, Moisture_t::Moist, Moisture_t::Wet})
            {
                Tile* pHost = nullptr;
                int bestDistance = searchRadius + 1;
                ForEachTileInChebyshevRadius(
                    rBaseTile, rMap, searchRadius, /*includeOrigin=*/false,
                    [&](Tile* pTile, int distance) {
                        if (!pTile || pTile->HasImprovement(ImprovementIds::k_Base)
                            || pTile->HasImprovement(ImprovementIds::k_Farm)
                            || pTile->GetMoisture() != moisture
                            || !CanBuildImprovement(*pTile, rFarm) || distance >= bestDistance)
                        {
                            return;
                        }
                        pHost = pTile;
                        bestDistance = distance;
                    });

                if (!pHost)
                {
                    throw std::runtime_error(
                        "Engine setup: no " + ToString(moisture)
                        + " tile near base for Farm");
                }

                rTileEffects.AddOccupantWithEffects(*pHost, rFarm.id);
                std::cout << "Placed Farm (" << ToString(moisture) << ") for faction "
                          << rFaction.GetFactionId() << " at (" << pHost->GetX() << ", "
                          << pHost->GetY() << ")\n";
            }
            break; // one base per faction is enough for the temporary showcase
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
    m_pSettings->Load();

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
            // Routed through the same exit flag as every other quit path, so "did the user
            // quit?" has one answer.
            // TODO: SMAC prompts to save before quitting; that UI flow does not exist yet, so
            // the close button quits immediately.
            m_uiManager->RequestExit();
            continue;
        }

        m_uiManager->ProcessInput();
        // Between input and paint: consumes UI-queued turn-advance requests (WorldView auto
        // end-turn) so Advance never runs from the Render path.
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
    // Turn processing can mutate/destroy pops (starvation) and other base state that
    // base-level popups (e.g. PopTypeSelectorPopup) hold live references to. UIManager's
    // modal/overlay contract (UIManager::CanAdvanceTurn) is the single source of truth for
    // whether it is safe to resume: an overlay is on the stack, or the world view reports a
    // blocking in-view modal (probe/supply popup, ...). This is a soft gate, not a
    // programmer-error assert — an ordinary player End Turn / Enter / auto-advance while a
    // modal is open is expected UI traffic, not a bypass of view-stack routing, so it simply
    // no-ops instead of throwing.
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

    // Three explicit phases. App data is process-wide and survives any number of sessions;
    // the session is everything a "new game" builds; the UI is bound to the session that
    // exists by then. Keeping them separate is what will let load-game reuse StartNewGame_'s
    // successor without re-running config parsing, and lets tests build a session without a
    // graphics backend.
    InitializeApp_();
    StartNewGame_();
    InitializeUi_();
}

void Engine::InitializeApp_()
{
    // Settings are already loaded: the constructor needs them to open the window.
    UiStyle::Load("config/ui/style.json");

    // Every config parser + cross-config id validation (including HasComponent conditions).
    // Returns complete or throws: nothing downstream has to check a member for null.
    m_gameDataContext = std::make_unique<GameDataContext>(LoadGameData());
    ValidateTerrainArtReferences(Style().tileRenderer.waterShading,
                                 *m_gameDataContext->improvementRegistry);

    // Settings name a difficulty by id; reject an unknown one here, where the message can
    // still point at the settings file, rather than from the first Faction constructor.
    m_gameDataContext->difficultyConfig->RequireForSession(
        m_pSettings->GetGameRules().difficultyId);
    m_gameDataContext->nativeLifeLevelConfig->RequireForSession(
        m_pSettings->GetGameRules().nativeLifeLevelId);
}

void Engine::StartNewGame_()
{
    // One session seed, resolved once and handed down. `seed == 0` in the map config means
    // "pick one"; resolving it here (rather than letting each sub-object reach for
    // std::random_device) is what makes a session reproducible — every per-faction random
    // choice derives from this value.
    // TODO: persist the resolved seed once a save system exists. It must not go back through
    // GameSettings::SetMapGeneration - that would turn `seed: 0` ("pick one") into a fixed seed
    // for every later new game.
    const MapGenerationConfig_t& rWorldConfig = m_pSettings->GetMapGeneration();
    m_sessionSeed = rWorldConfig.seed != 0 ? rWorldConfig.seed : std::random_device{}();
    std::cout << "Session seed: " << m_sessionSeed << "\n";

    // Generate world map and build the save-game state around it.
    WorldGenerator worldGen;
    const WorldGenPresetConfig_t& rPreset =
        m_gameDataContext->worldGenPresetRegistry->Get(rWorldConfig.presetId);
    m_pGameState = std::make_unique<GameState>(
        worldGen.Generate(rWorldConfig, rPreset, *m_gameDataContext->worldGenDecorationConfig,
                          m_gameDataContext->worldGenLandmarks,
                          *m_gameDataContext->improvementRegistry,
                          m_gameDataContext->elevationRules,
                          m_sessionSeed),
        *m_gameDataContext,
        *m_pSettings,
        // Distinct sub-stream from world generation, so changing map size does not shift
        // combat rolls (and vice versa).
        static_cast<uint32_t>(m_sessionSeed ^ 0x5BF03635u));
    std::cout << "Generated world map: " << m_pGameState->GetWorldMap().GetWidth() << "x" << m_pGameState->GetWorldMap().GetHeight() << "\n";

    m_eventBridge = std::make_unique<EventBridge>(m_pGameState->GetEventBus());
    m_pGameState->GetEventBus().Subscribe([](const GameEvent& event) {
        if (auto* pGained = std::get_if<EvBaseGainedPop>(&event))
        {
            std::cout << "[EVENT] Base " << pGained->baseId << " (Faction " << pGained->factionId
                      << ") gained a pop! New size: " << pGained->newSize << "\n";
        }
        else if (auto* pLost = std::get_if<EvBaseLostPop>(&event))
        {
            std::cout << "[EVENT] Base " << pLost->baseId << " (Faction " << pLost->factionId
                      << ") lost a pop! New size: " << pLost->newSize << "\n";
        }
    });

    // Create factions from config with a starting base each.
    // Player base prefers x=0 so horizontal wrap is easy to exercise in the viewport.
    // Spacing must respect k_MinBaseFoundingSeparation; extra factions must not reuse
    // an earlier slot (modulo over a short list used to place two factions on the same tile).
    const int centerY = m_pGameState->GetWorldMap().GetHeight() / 2;
    const int mapWidth = m_pGameState->GetWorldMap().GetWidth();
    std::vector<std::pair<int, int>> preferredStartPositions;
    for (int x = (centerY & 1); x < mapWidth; x += 10)
    {
        preferredStartPositions.push_back({x, centerY});
    }

    std::vector<const BaseManager*> placedBases;
    size_t positionIndex = 0;
    for (const FactionConfig_t& rFactionConfig : m_gameDataContext->factionRegistry->GetAll())
    {
        // Single-player for now: the first faction created is the human player, the rest are
        // AI-controlled. IsPlayerControlled() (rather than an index-0 convention) is what
        // GameState::GetPlayerFaction() searches for.
        const bool bNativeLife = IsNativeLifeFaction(rFactionConfig.identity.species);
        const bool bIsPlayerControlled = (positionIndex == 0) && !bNativeLife;
        auto pFaction = std::make_unique<Faction>(
            m_pGameState->AllocateFactionId(),
            bIsPlayerControlled,
            rFactionConfig,
            *m_gameDataContext,
            m_pGameState->GetWorldMap(),
            *m_pSettings,
            // Per-faction sub-stream of the session seed, so factions do not share a sequence
            // and adding one does not shift another's picks.
            static_cast<uint32_t>(m_sessionSeed + 0x9E3779B9u * (positionIndex + 1)));

        // Wire EventBridge to every base this faction ever introduces — founding (below),
        // future load, and capture/trade adopt alike — via the single Faction::OnBaseAdded
        // hook, rather than a per-call-site WireBase chore (see EventBridge::WireBase).
        pFaction->OnBaseAdded.Connect([this](BaseManager& rBase) { m_eventBridge->WireBase(rBase); });

        if (bNativeLife)
        {
            Faction& rFaction = m_pGameState->AddFaction(std::move(pFaction));
            m_eventBridge->WireFaction(rFaction);
            ++positionIndex;
            continue;
        }

        Tile* pStartTile = PickStartingBaseTile_(
            m_pGameState->GetWorldMap(), placedBases, preferredStartPositions);
        if (!pStartTile)
        {
            throw std::runtime_error(
                "Engine setup: no valid land tile for faction '" + rFactionConfig.id
                + "' starting base");
        }

        BaseManager* pBase = pFaction->CreateBase(
            m_pGameState->AllocateBaseId(), pFaction->SuggestBaseName(), pStartTile,
            m_pGameState->GetTileEffects(),
            m_pGameState->GetSecretProjectAvailability(),
            /*initialPopulation=*/3);
        placedBases.push_back(pBase);

        Faction& rFaction = m_pGameState->AddFaction(std::move(pFaction));
        // After AddFaction: the faction's id and subsystems are final, and the bridge captures
        // the id rather than the object.
        m_eventBridge->WireFaction(rFaction);
        rFaction.GetEconomy().AddEnergy(10);

        // Temporary test units so fog-of-war vision and specials are easy to exercise.
        {
            const UnitComponentRegistry& rComponents = *m_gameDataContext->unitComponentRegistry;
            const std::vector<UnitSlotConfig_t>& rSlots = m_gameDataContext->unitSlotRegistry->GetAll();

            auto resolve = [&rComponents](const std::string& rId) -> const UnitComponentConfig_t*
            {
                const UnitComponentConfig_t* pComponent = rComponents.Find(rId);
                if (!pComponent)
                {
                    throw std::runtime_error("Engine setup: unknown unit component '" + rId + "'");
                }
                return pComponent;
            };

            auto addDesign = [&rFaction](std::unique_ptr<UnitDesign> pDesign,
                                         const char* pLabel) -> const UnitDesign&
            {
                const UnitDesign& rDesign = *pDesign;
                if (!rFaction.GetMilitary().AddDesign(std::move(pDesign)))
                {
                    throw std::runtime_error(
                        std::string("Engine setup: failed to add ") + pLabel + " design");
                }
                return rDesign;
            };

            WorldMap& rMap = m_pGameState->GetWorldMap();
            UnitPositionIndex& rPositions = rMap.GetUnitPositions();

            std::unordered_map<std::string, const UnitComponentConfig_t*> basicParts = {
                {"chassis", resolve("HoverTank")},
                {"weapon",  resolve("Missile_Weapons")},
                {"armour",  resolve("No_Armour")},
                {"reactor", resolve("Fission_Plant")},
            };
            const UnitDesign& rBasicDesign = addDesign(
                std::make_unique<UnitDesign>(rSlots, basicParts), "basic scout");

            std::unordered_map<std::string, const UnitComponentConfig_t*> probeParts = {
                {"chassis", resolve("Infantry")},
                {"weapon",  resolve("Probe_Team")},
                {"armour",  resolve("No_Armour")},
                {"reactor", resolve("Fission_Plant")},
            };
            const UnitDesign& rProbeDesign = addDesign(
                std::make_unique<UnitDesign>(rSlots, probeParts), "probe team");

            if (bIsPlayerControlled)
            {
                std::unordered_map<std::string, const UnitComponentConfig_t*> colonyParts = {
                    {"chassis", resolve("Infantry")},
                    {"weapon",  resolve("Colony_Pod")},
                    {"armour",  resolve("No_Armour")},
                    {"reactor", resolve("Fission_Plant")},
                };
                const UnitDesign& rColonyDesign = addDesign(
                    std::make_unique<UnitDesign>(rSlots, colonyParts), "colony pod");

                std::unordered_map<std::string, const UnitComponentConfig_t*> crawlerParts = {
                    {"chassis", resolve("Infantry")},
                    {"weapon",  resolve("Supply_Crawler")},
                    {"armour",  resolve("No_Armour")},
                    {"reactor", resolve("Fission_Plant")},
                };
                const UnitDesign& rCrawlerDesign = addDesign(
                    std::make_unique<UnitDesign>(rSlots, crawlerParts), "supply crawler");

                std::unordered_map<std::string, const UnitComponentConfig_t*> needlejetParts = {
                    {"chassis", resolve("Needlejet")},
                    {"weapon",  resolve("Missile_Weapons")},
                    {"armour",  resolve("No_Armour")},
                    {"reactor", resolve("Fission_Plant")},
                };
                const UnitDesign& rNeedlejetDesign = addDesign(
                    std::make_unique<UnitDesign>(rSlots, needlejetParts), "needlejet");

                std::unordered_map<std::string, const UnitComponentConfig_t*> missileParts = {
                    {"chassis", resolve("Missile")},
                    {"weapon",  resolve("Missile_Weapons")},
                    {"armour",  resolve("No_Armour")},
                    {"reactor", resolve("Fission_Plant")},
                };
                const UnitDesign& rMissileDesign = addDesign(
                    std::make_unique<UnitDesign>(rSlots, missileParts), "missile");

                // Vision-1 HoverTank scout beside the base (Deep Radar would stack to 2).
                const Tile& rStart = *pStartTile;
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rBasicDesign, rPositions,
                    *GetTileAtLatticeOffset(rMap, rStart, 1, 0), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rColonyDesign, rPositions,
                    *GetTileAtLatticeOffset(rMap, rStart, 1, 1), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rCrawlerDesign, rPositions,
                    *GetTileAtLatticeOffset(rMap, rStart, 2, 1), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rProbeDesign, rPositions,
                    *GetTileAtLatticeOffset(rMap, rStart, 1, -1), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rNeedlejetDesign, rPositions,
                    rStart, pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rMissileDesign, rPositions,
                    rStart, pBase);
            }
            else
            {
                // Enemy scout / probe beside the AI base for multi-faction checks.
                const Tile& rStart = *pStartTile;
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rBasicDesign, rPositions,
                    *GetTileAtLatticeOffset(rMap, rStart, 1, 0), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rProbeDesign, rPositions,
                    *GetTileAtLatticeOffset(rMap, rStart, 1, -1), pBase);
            }
        }

        ++positionIndex;
    }

    // Temporary: extra player bases to eyeball land/water sprites and defense overlays.
    if (Faction* pPlayer = m_pGameState->GetPlayerFaction();
        pPlayer && !pPlayer->Bases().empty())
    {
        PlacePreviewFactionBases_(*m_pGameState, *pPlayer, placedBases);
    }

    // Bases are founded before AddFaction, so each faction's territory is folded in by
    // AttachToSession_'s catch-up sweep as it is registered. This final rebuild is the
    // whole-world pass once every faction exists.
    m_pGameState->RebuildTerritory();

    // Temporary: give the player commerce partners — first AI Pact, second AI Treaty.
    {
        Faction* pPlayer = m_pGameState->GetPlayerFaction();
        if (pPlayer == nullptr)
        {
            throw std::runtime_error("Engine setup: no player faction for starting diplomacy");
        }

        DiplomacyLedger& rDiplomacy = m_pGameState->GetDiplomacyLedger();
        const FactionId_t playerId = pPlayer->GetFactionId();
        int aiIndex = 0;
        for (Faction& rFaction : m_pGameState->Factions())
        {
            if (rFaction.GetFactionId() == playerId
                || IsNativeLifeFaction(rFaction.GetDefinition().identity.species))
            {
                continue;
            }

            rDiplomacy.SetKnown(playerId, rFaction.GetFactionId());
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

    m_pGameState->CreatePlanetaryCouncil();
    m_pGameState->CreateWorldEvents();
    m_councilAiVoteConn = m_pGameState->GetPlanetaryCouncil()->OnProposalOpened.ConnectScoped(
        [this](Faction& /*rProposer*/, const std::string& /*rProposalId*/) {
            if (PlanetaryCouncil* pCouncil = m_pGameState->GetPlanetaryCouncil())
            {
                CastStubCouncilVotes(*pCouncil);
            }
        });

#ifdef AC_PLACE_TEST_IMPROVEMENTS
    PlaceTestImprovementsAroundBases_(
        m_pGameState->GetWorldMap(), m_pGameState->GetTileEffects(),
        *m_gameDataContext->improvementRegistry, *m_pGameState);
#endif

    std::cout << "Test setup complete. " << m_pGameState->GetNumFactions() << " faction(s), "
              << m_pGameState->GetPlayerFaction()->GetBaseCount() << " base(s)\n";
}

void Engine::InitializeUi_()
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
        [this]() { ProcessTurn_(); },
        [this]() { m_uiManager->RequestExit(); },
        [this](BaseManager& rBase) { m_uiManager->PushView(m_viewFactory->CreateBaseView(rBase)); },
        [this](CombatResult_t result,
               const Tile& rAttackerTile,
               const Tile& rDefenderTile,
               std::string attackerName,
               std::string defenderName,
               WorldDisplay& rWorldDisplay,
               WindowLayout_t mapLayout,
               std::function<void()> onFinished) {
            m_uiManager->PushView(m_viewFactory->CreateCombatView(
                m_viewFactory->GetFullscreenLayout(),
                std::move(result),
                rAttackerTile,
                rDefenderTile,
                std::move(attackerName),
                std::move(defenderName),
                rWorldDisplay,
                mapLayout,
                std::move(onFinished)));
        },
        [this]() {
            const WindowLayout_t fullscreen = m_viewFactory->GetFullscreenLayout();
            m_uiManager->PushView(m_viewFactory->CreateCommlinksView(
                fullscreen,
                [this, fullscreen]() {
                    m_uiManager->PushView(m_viewFactory->CreateCouncilVoteView(fullscreen));
                }));
        }
    );
    WorldView& rWorldView = *pWorldView;
    const auto bindView = [&](HotkeyAction_t action, auto createView)
    {
        if (const std::optional<HotkeyChord_t> chord = m_pHotkeys->Find(action))
        {
            m_uiManager->RegisterViewShortcut(*chord, createView);
        }
    };
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

void Engine::PrintWelcome_() const
{
    std::cout << "Welcome to Alpha Centauri (C++ rebuild)!\n";
}

} // namespace ac
