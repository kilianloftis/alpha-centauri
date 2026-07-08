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
#include "game/EffectReferenceValidator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/ImprovementRegistry.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/faction/FactionRegistry.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechCostCalculator.h"
#include "game/faction/base/production/ProductionCostConfig.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
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
    m_gameDataContext->popTypeAvailabilityCalculator =
        std::make_unique<PopTypeAvailabilityCalculator>(*m_gameDataContext->popTypeRegistry);

    m_gameDataContext->buildingRegistry = std::make_unique<BuildingRegistry>();
    m_gameDataContext->buildingRegistry->Load("config/buildings");

    m_gameDataContext->improvementRegistry = std::make_unique<ImprovementRegistry>();
    m_gameDataContext->improvementRegistry->Load("config/improvements.json");

    m_gameDataContext->unitComponentRegistry = std::make_unique<UnitComponentRegistry>();
    m_gameDataContext->unitComponentRegistry->Load("config/unit_components");

    m_gameDataContext->unitSlotRegistry = std::make_unique<UnitSlotRegistry>();
    m_gameDataContext->unitSlotRegistry->Load("config/unit_slot_config.json");

    m_gameDataContext->techRegistry = std::make_unique<TechRegistry>();
    m_gameDataContext->techRegistry->Load("config/techs.json");

    m_gameDataContext->socialPolicyRegistry = std::make_unique<SocialPolicyRegistry>();
    m_gameDataContext->socialPolicyRegistry->Load("config/social_policies.json");

    m_gameDataContext->socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
    m_gameDataContext->socialRatingRegistry->Load("config/social_rating_effects.json");

    m_gameDataContext->factionRegistry = std::make_unique<FactionRegistry>();
    m_gameDataContext->factionRegistry->Load("config/factions");

    // All effect-declaring registries are loaded; fail startup on any effect that
    // references a nonexistent building/tech/improvement/feature id.
    ValidateEffectReferences(*m_gameDataContext);

    m_gameDataContext->luaRuntime = std::make_unique<LuaRuntime>();

    PopCompositionConfigParser compositionParser;
    m_gameDataContext->popCompositionConfig =
        std::make_unique<PopCompositionConfig>(
            compositionParser.ParseConfig("config/pop_composition.lua", *m_gameDataContext->luaRuntime));
    m_gameDataContext->popCompositionCalculator =
        std::make_unique<PopCompositionCalculator>(
            *m_gameDataContext->popCompositionConfig, *m_gameDataContext->luaRuntime);

    GrowthConfig_tParser growthParser;
    m_gameDataContext->growthConfig =
        std::make_unique<GrowthConfig_t>(
            growthParser.ParseConfig("config/pop_growth.json"));
    TechCostConfigParser techCostParser;
    m_gameDataContext->techCostConfig =
        std::make_unique<TechCostConfig>(
            techCostParser.ParseConfig("config/tech_cost.lua", *m_gameDataContext->luaRuntime));
    m_gameDataContext->techCostCalculator =
        std::make_unique<TechCostCalculator>(
            *m_gameDataContext->techCostConfig, *m_gameDataContext->luaRuntime);

    ProductionCostConfig_tParser productionCostParser;
    m_gameDataContext->productionCostConfig =
        std::make_unique<ProductionCostConfig_t>(
            productionCostParser.ParseConfig("config/production_cost.lua", *m_gameDataContext->luaRuntime));
    m_gameDataContext->productionCostCalculator =
        std::make_unique<ProductionCostCalculator>(
            *m_gameDataContext->productionCostConfig, *m_gameDataContext->luaRuntime);

    // Generate world map
    WorldGenerator worldGen;
    WorldGenConfig worldConfig;
    worldConfig.width = 30;
    worldConfig.height = 20;
    worldConfig.minElevation = -1000;
    worldConfig.maxElevation = 4000;
    m_gameState->SetWorldMap(worldGen.Generate(worldConfig));
    std::cout << "Generated world map: " << m_gameState->GetWorldMap()->GetWidth() << "x" << m_gameState->GetWorldMap()->GetHeight() << "\n";

    m_gameState->InitTileEffects(*m_gameDataContext->improvementRegistry,
                                 m_gameDataContext->unitComponentRegistry.get());

    m_gameDataContext->secretProjectAvailabilityCalculator =
        std::make_unique<SecretProjectAvailabilityCalculator>(m_gameState->GetFactions());

    // Create factions from config with a starting base each.
    const int centerX = m_gameState->GetWorldMap()->GetWidth() / 2;
    const int centerY = m_gameState->GetWorldMap()->GetHeight() / 2;
    const std::vector<std::pair<int, int>> startPositions = {
        {centerX, centerY},
        {centerX + 3, centerY + 3},
    };

    int factionId = 1;
    int baseId = 1;
    size_t positionIndex = 0;
    for (const FactionConfig_t& rFactionConfig : m_gameDataContext->factionRegistry->GetAll())
    {
        auto pFaction = std::make_unique<Faction>(
            m_gameDataContext->buildingRegistry.get(),
            m_gameDataContext->techRegistry.get(),
            m_gameDataContext->socialPolicyRegistry.get(),
            m_gameDataContext->socialRatingRegistry.get(),
            m_gameDataContext->techCostCalculator.get(),
            m_gameDataContext->popTypeAvailabilityCalculator.get(),
            &rFactionConfig);

        const auto& [startX, startY] = startPositions[positionIndex % startPositions.size()];
        BaseManager* pBase = pFaction->CreateBase(
            factionId, baseId, pFaction->SuggestBaseName(),
            m_gameState->GetWorldMap()->GetTile(startX, startY),
            *m_gameDataContext,
            m_gameState->GetTileEffects());

        m_eventBridge->WireBase(*pBase);
        m_gameState->AddFaction(std::move(pFaction));

        ++factionId;
        ++baseId;
        ++positionIndex;
    }

    std::cout << "Test setup complete. " << m_gameState->GetNumFactions() << " faction(s), "
              << m_gameState->GetPlayerFaction()->GetBaseCount() << " base(s)\n";

    m_turnStageFactory = std::make_unique<TurnStageFactory>();
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
    m_uiManager->RegisterViewShortcut(Key_t::E, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateSocialEngineeringView(fullscreen);
    });
    m_uiManager->RegisterViewShortcut(Key_t::U, [this, fullscreen]() -> std::unique_ptr<IGameView> {
        return m_viewFactory->CreateUnitDesignerView(fullscreen);
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
