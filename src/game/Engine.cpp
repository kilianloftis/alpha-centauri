#include "game/Engine.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/TurnStageFactory.h"
#include "game/TurnProcessor.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "input/KeyMapping.h"
#include "lib/EventBus.h"
#include "lib/EventBridge.h"
#include "lib/GameEvent.h"
#include "game/faction/base/BaseManager.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/pop-types/PopTypeRegistry.h"
#include "game/faction/base/population/pop-types/PopCompositionConfigParser.h"
#include "game/faction/base/population/pop-types/GrowthConfigParser.h"
#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include "game/faction/base/population/calculators/GrowthCalculator.h"
#include "lib/LuaRuntime.h"
#include "ui/WorldDisplay.h"
#include "ui/BaseWorkableAreaDisplay.h"
#include "ui/TileHitTester.h"
#include "game/map/WorldGenerator.h"
#include "ui/UIManager.h"
#include "ui/UIPanel.h"
#include "ui/UIWorldMap.h"
#include <iostream>
#include <stdexcept>

namespace ac
{

Engine::Engine()
    : m_graphics(CreateGraphics())
    , m_input(CreateInput())
    , m_turnStageFactory(std::make_unique<TurnStageFactory>())
    , m_gameState(std::make_unique<GameState>())
    , m_eventBus(std::make_unique<EventBus>())
    , m_gameDataContext(std::make_unique<GameDataContext>())
    , m_uiManager(CreateUIManager())
    {}

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
    while (!m_bShouldExit)
    {
        m_graphics->Clear();
        Render_();
        // Update UI
        m_uiManager->Update(0.f);

        // Display current game state
        m_graphics->Clear();

        // Populate info panel with current game state
        {
            std::vector<UIPanel::InfoLine> infoLines;
            infoLines.push_back({"Mission Year: " + std::to_string(m_gameState->GetMissionYear()), Color::White()});
            const auto& factions = m_gameState->GetFactions();
            if (!factions.empty())
            {
                const Faction* pPlayerFaction = factions[0].get();
                infoLines.push_back({"Energy: " + std::to_string(pPlayerFaction->GetEnergy()), Color::Yellow()});
                infoLines.push_back({"Research: " + std::to_string(pPlayerFaction->GetResearchPoints()), Color{100, 200, 255, 255}});
            }
            m_uiManager->GetInfoPanel().SetInfoLines(infoLines);
        }

        // Draw UI layers (world map -> info panel -> popups)
        m_uiManager->Draw(*m_graphics);
        m_graphics->Display();

        HandleMouseInput_();
        HandleKeyInput_();
    }
}

void Engine::ProcessTurn_()
{
    m_eventBus->publish(EvTurnStarted{ m_gameState->GetMissionYear() });
    m_turnProcessor->ProcessTurn(m_gameState->GetMissionYear(), m_gameState->GetNumFactions(), *m_gameState);
    m_gameState->IncrementMissionYear();
}

void Engine::Initialize_()
{
    std::cout << "Initializing game engine...\n";

    // Initialize graphics backend
    if (!m_graphics->Initialize())
    {
        throw std::runtime_error("Failed to initialize graphics backend");
    }

    // Create EventBridge
    m_eventBridge = std::make_unique<EventBridge>(*m_eventBus);

    // Subscribe to population events for testing
    m_eventBus->subscribe([](const GameEvent& event) {
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

    m_gameDataContext->popTypeRegistry = std::make_unique<PopTypeRegistry>();
    m_gameDataContext->popTypeRegistry->Load("config/pop_types.json");

    m_gameDataContext->buildingRegistry = std::make_unique<BuildingRegistry>();
    m_gameDataContext->buildingRegistry->Load("config/buildings.json");

    m_gameDataContext->techRegistry = std::make_unique<TechRegistry>();
    m_gameDataContext->techRegistry->Load("config/techs.json");

    m_gameDataContext->luaRuntime = std::make_unique<LuaRuntime>();

    PopCompositionConfigParser compositionParser;
    m_gameDataContext->popCompositionConfig =
        std::make_unique<PopCompositionConfig>(
            compositionParser.ParseConfig("config/pop_composition.lua", *m_gameDataContext->luaRuntime));
    m_gameDataContext->popCompositionCalculator =
        std::make_unique<PopCompositionCalculator>(
            *m_gameDataContext->popCompositionConfig, *m_gameDataContext->luaRuntime);

    GrowthConfigParser growthParser;
    m_gameDataContext->growthConfig =
        std::make_unique<GrowthConfig>(
            growthParser.ParseConfig("config/pop_growth.lua", *m_gameDataContext->luaRuntime));
    m_gameDataContext->growthCalculator =
        std::make_unique<GrowthCalculator>(
            *m_gameDataContext->growthConfig, *m_gameDataContext->luaRuntime);

    // Generate world map
    WorldGenerator worldGen;
    WorldGenConfig worldConfig;
    worldConfig.width = 12;
    worldConfig.height = 8;
    worldConfig.minElevation = -1000;
    worldConfig.maxElevation = 2000;
    m_gameState->SetWorldMap(worldGen.Generate(worldConfig));
    std::cout << "Generated world map: " << m_gameState->GetWorldMap()->GetWidth() << "x" << m_gameState->GetWorldMap()->GetHeight() << "\n";

    // Create test faction with a base
    auto pFaction = std::make_unique<Faction>();
    auto pBase = std::make_unique<BaseManager>(m_gameDataContext->buildingRegistry.get());
    pBase->SetFactionId(1);  // Test faction ID
    pBase->SetBaseId(1);     // Test base ID
    pBase->SetName("Test Base");
    pBase->SetPosition(6, 4);  // Center of 12x8 world

    // Inject pop type registry and composition calculator into base population
    pBase->SetPopRegistry(m_gameDataContext->popTypeRegistry.get());
    pBase->SetPopCompositionCalculator(m_gameDataContext->popCompositionCalculator.get());

    // Auto-assign initial workers to tiles
    pBase->AutoAssignWorkers();

    // Set up tile lookup for resource calculations
    if (m_gameState->GetWorldMap())
    {
        pBase->SetTileLookup([this](int x, int y) -> const Tile* {
            return m_gameState->GetWorldMap()->GetTile(x, y);
        });
    }

    std::cout << "Created test base with population: " << pBase->GetBaseSize()
              << " (workers: " << pBase->GetPopWorkerCount() << ")\n";

    // Wire base signals to EventBus
    m_eventBridge->WireBase(*pBase);

    // Add base to faction
    pFaction->AddBase(std::move(pBase));
    m_gameState->AddFaction(std::move(pFaction));

    std::cout << "Test setup complete. " << m_gameState->GetNumFactions() << " faction(s), "
              << m_gameState->GetFactions()[0]->GetBaseCount() << " base(s)\n";

    // Create UI displays
    m_worldDisplay = std::make_unique<WorldDisplay>(*m_graphics);
    m_worldDisplay->SetWorldMap(m_gameState->GetWorldMap());

    // Collect base positions for the world display
    std::vector<std::pair<int, int>> basePositions;
    for (const auto& pFaction : m_gameState->GetFactions())
    {
        for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
        {
            const BaseManager* pB = pFaction->GetBase(i);
            if (pB)
            {
                basePositions.emplace_back(pB->GetX(), pB->GetY());
            }
        }
    }
    m_worldDisplay->SetBasePositions(basePositions);

    m_workableAreaDisplay = std::make_unique<BaseWorkableAreaDisplay>(*m_graphics, *m_gameState->GetWorldMap());

    m_turnStageFactory->SetCompositionCalculator(m_gameDataContext->popCompositionCalculator.get());
    m_turnStageFactory->SetGrowthCalculator(m_gameDataContext->growthCalculator.get());
    m_turnStageFactory->LoadConfig("config/turn_stages.json");
    auto registry = m_turnStageFactory->CreateStages();
    TurnStageRepeatFlags_t repeatFlags;
    std::vector<std::string> stageOrder;
    for (const auto& config : m_turnStageFactory->GetStageConfigs())
    {
        repeatFlags[config.id] = config.repeat_for_each_faction;
        stageOrder.push_back(config.id);
    }
    m_turnProcessor = std::make_unique<TurnProcessor>(std::move(registry), std::move(repeatFlags), std::move(stageOrder));

    if (m_uiManager)
    {
        m_uiManager->Initialize(*m_graphics);
        m_uiManager->GetWorldMap().SetWorldDisplay(m_worldDisplay.get());
    }
    CheckInitialized_();
}

void Engine::CheckInitialized_() const
{
if (!m_graphics)
    {
        std::cout << "No graphics backend available\n";
        throw std::runtime_error("Failed to create graphics backend");
    }
if (!m_input)
    {
        std::cout << "No input backend available\n";
        throw std::runtime_error("Failed to create input backend");
    }
}

void Engine::PrintWelcome_() const
{
    std::cout << "Welcome to Alpha Centauri (C++ rebuild)!\n";
}

void Engine::HandleMouseInput_()
{
    m_input->CaptureMouseAsync([this](MouseEvent_t event)
    {
        if (event.button == MouseButton_t::None)
        {
            return;
        }

        switch (m_activeView)
        {
            case ViewMode::World:
                HandleWorldViewMouse_(event.x, event.y);
                break;
            case ViewMode::Base:
                HandleBaseViewMouse_(event.x, event.y);
                break;
        }
    });
}

void Engine::HandleKeyInput_()
{
    m_input->CaptureKeyAsync([this](KeyEvent_t event)
    {
        if (event.key == Key_t::Escape)
        {
            if (m_activeView == ViewMode::Base)
            {
                ReturnToWorldView_();
            }
            else
            {
                m_bShouldExit = true;
            }
        }
        else if (event.key == Key_t::Enter && m_activeView == ViewMode::World)
        {
            ProcessTurn_();
        }
    });
}

void Engine::Render_()
{
    switch (m_activeView)
    {
        case ViewMode::World:
            RenderWorldView_();
            break;
        case ViewMode::Base:
            RenderBaseView_();
            break;
    }

    // Show last-clicked tile info at the bottom of the screen
    if (!m_lastClickedTileText.empty())
    {
        m_graphics->DrawText(m_lastClickedTileText, 20.f, 570.f, 18, Color::Yellow());
    }
}

void Engine::RenderWorldView_()
{
    if (m_worldDisplay)
    {
        m_worldDisplay->Render(kWorldOriginX, kWorldOriginY, kWorldTileSize);
    }
}

void Engine::RenderBaseView_()
{
    if (m_pActiveBase && m_workableAreaDisplay)
    {
        m_graphics->DrawText(m_pActiveBase->GetName(), 20.f, 40.f, 20, Color::Yellow());

        const std::string nutrientText = "Nutrients: " + std::to_string(m_pActiveBase->GetNutrientStockpile());
        const std::string mineralText  = "Minerals:  " + std::to_string(m_pActiveBase->GetMineralStockpile());
        const std::string energyText   = "Energy:    " + std::to_string(m_pActiveBase->GetEnergyProduction()) + "/turn";
        m_graphics->DrawText(nutrientText, 20.f, 70.f, 16, Color::White());
        m_graphics->DrawText(mineralText,  20.f, 90.f, 16, Color::White());
        m_graphics->DrawText(energyText,   20.f, 110.f, 16, Color::White());

        m_workableAreaDisplay->Render(kBaseAreaCenterX, kBaseAreaCenterY, kBaseTileSize);
    }
}

void Engine::HandleWorldViewMouse_(int mouseX, int mouseY)
{
    const WorldMap* pWorldMap = m_gameState->GetWorldMap();
    if (!pWorldMap)
    {
        return;
    }

    const UIWorldMap& rWorldMapUI = m_uiManager->GetWorldMap();
    const float tileSize = std::min(
        rWorldMapUI.GetWidth()  / static_cast<float>(pWorldMap->GetWidth()),
        rWorldMapUI.GetHeight() / static_cast<float>(pWorldMap->GetHeight()));

    auto tile = TileHitTester::HitTestWorldGrid(
        static_cast<float>(mouseX), static_cast<float>(mouseY),
        rWorldMapUI.GetX(), rWorldMapUI.GetY(), tileSize,
        pWorldMap->GetWidth(), pWorldMap->GetHeight());

    if (tile)
    {
        m_lastClickedTile = tile;
        m_lastClickedTileText = "Clicked tile: (" + std::to_string(tile->first) + ", " + std::to_string(tile->second) + ")";

        BaseManager* pBase = FindBaseAtTile_(tile->first, tile->second);
        if (pBase)
        {
            OpenBaseView_(pBase);
        }
    }
    else
    {
        m_lastClickedTile = std::nullopt;
        m_lastClickedTileText = "Clicked: (" + std::to_string(mouseX) + ", " + std::to_string(mouseY) + ") - no tile";
    }
}

void Engine::HandleBaseViewMouse_(int mouseX, int mouseY)
{
    if (!m_pActiveBase)
    {
        return;
    }

    auto tile = TileHitTester::HitTestBaseWorkableArea(
        static_cast<float>(mouseX), static_cast<float>(mouseY),
        kBaseAreaCenterX, kBaseAreaCenterY, kBaseTileSize,
        m_pActiveBase->GetX(), m_pActiveBase->GetY());

    if (!tile)
    {
        m_lastClickedTile = std::nullopt;
        m_lastClickedTileText = "Clicked: (" + std::to_string(mouseX) + ", " + std::to_string(mouseY) + ") - no tile";
        return;
    }

    m_lastClickedTile = tile;
    int tileX = tile->first;
    int tileY = tile->second;
    auto& rAssignments = m_pActiveBase->GetWorkerAssignments();
    const auto& rPops = m_pActiveBase->GetPopContainer();

    if (rAssignments.IsTileAssigned(tileX, tileY))
    {
        // Unassign the worker from this tile
        for (const auto& rEntry : rAssignments.GetAssignments())
        {
            if (rEntry.second.first == tileX && rEntry.second.second == tileY)
            {
                rAssignments.UnassignWorker(rEntry.first);
                m_lastClickedTileText = "Unassigned worker from (" + std::to_string(tileX) + ", " + std::to_string(tileY) + ")";
                return;
            }
        }
    }
    else
    {
        // Assign an unassigned worker to this tile
        const auto& pops = rPops.GetPops();
        for (int i = static_cast<int>(pops.size()) - 1; i >= 0; --i)
        {
            const Pop* pPop = pops[i].get();
            if (pPop->IsWorker() && rAssignments.GetAssignedTile(pPop->GetId()).first == -1)
            {
                rAssignments.UnassignWorker(pPop->GetId());
                if (rAssignments.AssignWorker(pPop->GetId(), tileX, tileY, rPops))
                {
                    m_lastClickedTileText = "Reassigned worker to (" + std::to_string(tileX) + ", " + std::to_string(tileY) + ")";
                    return;
                }
            }
        }
        m_lastClickedTileText = "No workers available to reassign";
    }
}

BaseManager* Engine::FindBaseAtTile_(int tileX, int tileY) const
{
    for (const auto& pFaction : m_gameState->GetFactions())
    {
        for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
        {
            BaseManager* pBase = pFaction->GetBase(i);
            if (pBase && pBase->GetX() == tileX && pBase->GetY() == tileY)
            {
                return pBase;
            }
        }
    }
    return nullptr;
}

void Engine::OpenBaseView_(BaseManager* pBase)
{
    m_pActiveBase = pBase;
    m_activeView = ViewMode::Base;
    m_workableAreaDisplay->SetBase(pBase);
    m_lastClickedTileText = "Base: " + pBase->GetName();
}

void Engine::ReturnToWorldView_()
{
    m_activeView = ViewMode::World;
    m_pActiveBase = nullptr;
    m_lastClickedTileText.clear();
}

} // namespace ac
