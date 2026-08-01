#include "game/Engine.h"
#include "game/GameState.h"
#include "game/GameSettings.h"
#include "game/Faction.h"
#include "game/TurnStageFactory.h"
#include "game/TurnProcessor.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "input/KeyMapping.h"
#include "lib/EventBus.h"
#include "game/EventBridge.h"
#include "lib/GameEvent.h"
#include "game/faction/base/BaseManager.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/MapUtils.h"
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
#include "ui/IGameView.h"
#include "ui/TileHitTester.h"
#include "game/map/MapGenerationConfig.h"
#include "game/map/WorldGenPresetRegistry.h"
#include "game/map/WorldGenerator.h"
#include "ui/UIManager.h"
#include "ui/ViewFactory.h"
#include "ui/style/UiStyle.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

Engine::Engine()
    : m_pGraphics(CreateGraphics())
    , m_pInput(CreateInput())
    , m_pSettings(std::make_unique<GameSettings>())
    , m_gameDataContext(std::make_unique<GameDataContext>())
{
    if (!m_pGraphics)
    {
        throw std::runtime_error("Failed to create graphics backend");
    }
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
        m_uiManager->ProcessInput();
        m_uiManager->Render();
    }
}

void Engine::ProcessTurn_()
{
    // Turn processing can mutate/destroy pops (starvation) and other base state that
    // base-level popups (e.g. PopTypeSelectorPopup) hold live references to. Input routing
    // already makes this unreachable in practice - UIManager only routes input to the top of
    // the overlay stack, and this callback is only ever wired to WorldView's Enter handler -
    // but that makes it an implicit invariant. Assert it explicitly so a future caller that
    // bypasses view-stack routing (a scripted/auto turn, say) fails loudly instead of silently
    // dangling a popup's captured reference.
    if (m_uiManager->HasOverlayView())
    {
        throw std::logic_error("Engine::ProcessTurn_ called while an overlay view is active");
    }

    // Runs until a stage yields for interaction; turn boundaries are handled inside stages.
    m_turnProcessor->Advance(*m_pGameState);
}

void Engine::Initialize_()
{
    std::cout << "Initializing game engine...\n";

    m_pSettings->Load();

    UiStyle::Load("config/ui/style.json");

    // Every config parser + cross-config id validation (including unitFilter HasComponent).
    LoadGameData(*m_gameDataContext);

    // Generate world map and build the save-game state around it.
    WorldGenerator worldGen;
    const MapGenerationConfig_t& rWorldConfig = m_pSettings->GetMapGeneration();
    const WorldGenPresetConfig_t& rPreset =
        m_gameDataContext->worldGenPresetRegistry->Get(rWorldConfig.presetId);
    m_pGameState = std::make_unique<GameState>(
        worldGen.Generate(rWorldConfig, rPreset, *m_gameDataContext->worldGenDecorationConfig,
                          m_gameDataContext->worldGenLandmarks,
                          *m_gameDataContext->improvementRegistry),
        *m_gameDataContext->improvementRegistry,
        m_gameDataContext->unitComponentRegistry.get(),
        *m_pSettings,
        *m_gameDataContext->moraleCalculator);
    m_pGameState->GetDiplomaticActionExecutor().SetGameDataContext(*m_gameDataContext);
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
    // Player base at x=0 so horizontal wrap is easy to exercise in the viewport.
    const int centerX = m_pGameState->GetWorldMap().GetWidth() / 2;
    const int centerY = m_pGameState->GetWorldMap().GetHeight() / 2;
    const std::vector<std::pair<int, int>> startPositions = {
        {0, centerY},
        {centerX + 3, centerY + 3},
    };

    size_t positionIndex = 0;
    for (const FactionConfig_t& rFactionConfig : m_gameDataContext->factionRegistry->GetAll())
    {
        // Single-player for now: the first faction created is the human player, the rest are
        // AI-controlled. IsPlayerControlled() (rather than an index-0 convention) is what
        // GameState::GetPlayerFaction() searches for.
        const bool bIsPlayerControlled = (positionIndex == 0);
        auto pFaction = std::make_unique<Faction>(
            m_pGameState->AllocateFactionId(),
            bIsPlayerControlled,
            rFactionConfig,
            *m_gameDataContext);

        const auto& [startX, startY] = startPositions[positionIndex % startPositions.size()];
        BaseManager* pBase = pFaction->CreateBase(
            m_pGameState->AllocateBaseId(), pFaction->SuggestBaseName(),
            m_pGameState->GetWorldMap().GetTile(startX, startY),
            *m_gameDataContext,
            m_pGameState->GetTileEffects(),
            m_pGameState->GetSecretProjectAvailability());

        m_eventBridge->WireBase(*pBase);
        Faction& rFaction = m_pGameState->AddFaction(std::move(pFaction));

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

            if (bIsPlayerControlled)
            {
                std::unordered_map<std::string, const UnitComponentConfig_t*> radarParts = basicParts;
                radarParts["ability_1"] = resolve("Deep_Radar");
                const UnitDesign& rRadarDesign = addDesign(
                    std::make_unique<UnitDesign>(rSlots, radarParts), "deep-radar scout");

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

                // Basic vision-1 scout beside the base; deep-radar (vision 2) a little farther out.
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rBasicDesign, rPositions,
                    *rMap.GetTile(startX + 1, startY), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rRadarDesign, rPositions,
                    *rMap.GetTile(startX + 2, startY), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rColonyDesign, rPositions,
                    *rMap.GetTile(startX + 1, startY + 1), pBase);
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rCrawlerDesign, rPositions,
                    *rMap.GetTile(startX + 2, startY + 1), pBase);
            }
            else
            {
                // Enemy scout beside the AI base for multi-faction visibility/combat checks.
                rFaction.GetUnitManager().CreateUnit(
                    m_pGameState->AllocateUnitId(), rBasicDesign, rPositions,
                    *rMap.GetTile(startX + 1, startY), pBase);
            }
        }

        ++positionIndex;
    }

    // Bases were founded before AddFaction wired OnBaseListChanged, so rebuild once now.
    m_pGameState->RebuildTerritory();

    m_pGameState->CreatePlanetaryCouncil(*m_gameDataContext->councilProposalRegistry,
                                         *m_gameDataContext->councilRules);

    // Temporary test Sensors: one in each faction's territory (south of their starting base).
    {
        WorldMap& rMap = m_pGameState->GetWorldMap();
        TileEffectsContext& rTileEffects = m_pGameState->GetTileEffects();
        for (Faction& rFaction : m_pGameState->Factions())
        {
            for (BaseManager& rBase : rFaction.Bases())
            {
                const Tile& rBaseTile = rBase.GetTile();
                Tile* pSensorTile = rMap.GetTile(rBaseTile.GetX(), rBaseTile.GetY() + 2);
                if (!pSensorTile)
                {
                    throw std::runtime_error("Engine setup: Sensor tile out of bounds");
                }
                rTileEffects.AddImprovementWithEffects(*pSensorTile, "Sensor");
                std::cout << "Placed Sensor for faction " << rFaction.GetFactionId()
                          << " at (" << pSensorTile->GetX() << ", " << pSensorTile->GetY()
                          << "), territory owner "
                          << rMap.GetTerritory().GetOwner(*pSensorTile) << "\n";
                break;
            }
        }
        // Sensors are vision sources; rebuild fog after placing them.
        for (Faction& rFaction : m_pGameState->Factions())
        {
            rFaction.RebuildVisibility();
        }
    }

    // Temporary: fungus around the player base (and a couple forests) so vegetation fills
    // are easy to see while sprites are not yet in place.
    if (Faction* pPlayer = m_pGameState->GetPlayerFaction())
    {
        WorldMap& rMap = m_pGameState->GetWorldMap();
        TileEffectsContext& rTileEffects = m_pGameState->GetTileEffects();
        for (BaseManager& rBase : pPlayer->Bases())
        {
            ForEachTileInChebyshevRadius(rBase.GetTile(), rMap, 2, /*includeOrigin=*/false,
                [&](Tile* pTile, int distance)
                {
                    if (!pTile || !pTile->IsLand() || pTile->HasImprovement("Base"))
                    {
                        return;
                    }
                    else if (distance == 1
                             && (pTile->GetX() + pTile->GetY()) % 2 == 0
                             && !pTile->GetHasFungus())
                    {
                        rTileEffects.AddImprovementWithEffects(*pTile, "Forest");
                    }
                });
            break;
        }
    }

    std::cout << "Test setup complete. " << m_pGameState->GetNumFactions() << " faction(s), "
              << m_pGameState->GetPlayerFaction()->GetBaseCount() << " base(s)\n";

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

    m_viewFactory = std::make_unique<ViewFactory>(
        *m_pGameState,
        *m_gameDataContext,
        *m_pGraphics,
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
            m_uiManager->PushView(
                m_viewFactory->CreateCommlinksView(m_viewFactory->GetFullscreenLayout()));
        },
        [this](BaseManager& rBase) { m_eventBridge->WireBase(rBase); }
    );
    m_uiManager->RegisterViewShortcut(Key_t::F2, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateResearchView(fullscreen);
    });
    m_uiManager->RegisterViewShortcut(Key_t::E, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateSocialEngineeringView(fullscreen);
    });
    m_uiManager->RegisterViewShortcut(Key_t::U, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateUnitDesignerView(fullscreen);
    });
    m_uiManager->RegisterViewShortcut(Key_t::O, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateSettingsView(fullscreen);
    });
    m_uiManager->SetWorldView(std::move(pWorldView));

    // Start processing until the first interactive yield.
    m_turnProcessor->Advance(*m_pGameState);
}

void Engine::PrintWelcome_() const
{
    std::cout << "Welcome to Alpha Centauri (C++ rebuild)!\n";
}

} // namespace ac
