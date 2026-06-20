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
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechCostCalculator.h"
#include "lib/LuaRuntime.h"
#include "ui/IGameView.h"
#include "ui/TileHitTester.h"
#include "game/map/WorldGenerator.h"
#include "ui/UIManager.h"
#include "ui/ViewFactory.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ac
{

Engine::Engine()
    : m_graphics(CreateGraphics())
    , m_input(CreateInput())
    , m_gameState(std::make_unique<GameState>())
    , m_eventBus(std::make_unique<EventBus>())
    , m_gameDataContext(std::make_unique<GameDataContext>())
    , m_uiManager(std::make_unique<UIManager>())
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
    while (!m_uiManager->ShouldExit())
    {
        m_uiManager->ProcessInput();
        m_uiManager->Render();
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

    TechCostConfigParser techCostParser;
    m_gameDataContext->techCostConfig =
        std::make_unique<TechCostConfig>(
            techCostParser.ParseConfig("config/tech_cost.lua", *m_gameDataContext->luaRuntime));
    m_gameDataContext->techCostCalculator =
        std::make_unique<TechCostCalculator>(
            *m_gameDataContext->techCostConfig, *m_gameDataContext->luaRuntime);

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
    auto pFaction = std::make_unique<Faction>(m_gameDataContext->techRegistry.get(),
                                              m_gameDataContext->socialPolicyRegistry.get(),
                                              m_gameDataContext->techCostCalculator.get(),
                                              m_gameDataContext->popTypeRegistry.get());
    BaseManager* pBase = pFaction->CreateBase(
        1, 1, "Test Base", 6, 4,  // factionId, baseId, name, x, y (center of 12x8 world)
        *m_gameDataContext,
        *m_gameState->GetWorldMap());

    // Wire base signals to EventBus
    m_eventBridge->WireBase(*pBase);

    m_gameState->AddFaction(std::move(pFaction));

    std::cout << "Test setup complete. " << m_gameState->GetNumFactions() << " faction(s), "
              << m_gameState->GetPlayerFaction()->GetBaseCount() << " base(s)\n";

    m_turnStageFactory = std::make_unique<TurnStageFactory>(
        m_gameDataContext->popCompositionCalculator.get(),
        m_gameDataContext->growthCalculator.get());
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

    m_viewFactory = std::make_unique<ViewFactory>(
        *m_gameState,
        *m_gameDataContext,
        *m_graphics);

    m_uiManager->Initialize(*m_graphics, *m_input);
    const WindowLayout_t fullscreen = m_viewFactory->GetFullscreenLayout();

    auto pWorldView = m_viewFactory->CreateWorldView(
        fullscreen,
        [this]() { ProcessTurn_(); },
        [this]() { m_uiManager->RequestExit(); },
        [this](BaseManager& rBase) { m_uiManager->PushView(m_viewFactory->CreateBaseView(rBase)); }
    );
    m_uiManager->RegisterViewShortcut(Key_t::F2, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateResearchView(fullscreen);
    });
    m_uiManager->SetWorldView(std::move(pWorldView));
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

} // namespace ac
