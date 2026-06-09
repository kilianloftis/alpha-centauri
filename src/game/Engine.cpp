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
#include "game/faction/Base.h"
#include "game/faction/population/PopulationManager.h"
#include "game/faction/population/PopTypeRegistry.h"
#include "game/faction/population/PopCompositionConfigParser.h"
#include "game/faction/population/PopCompositionCalculator.h"
#include "lib/LuaRuntime.h"
#include "ui/PopulationDisplay.h"
#include <iostream>
#include <stdexcept>
#include <magic_enum.hpp>

namespace ac
{

Engine::Engine()
    : m_graphics(CreateGraphics())
    , m_input(CreateInput())
    , m_turnStageFactory(std::make_unique<TurnStageFactory>())
    , m_gameState(std::make_unique<GameState>())
    , m_eventBus(std::make_unique<EventBus>())
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
    while (!m_gameState->ShouldExit())
    {
        // Display current game state
        m_graphics->Clear();
        Render_();
        m_graphics->Display();

        // Wait for player input
        m_input->CaptureKeyAsync([this](KeyEvent event)
        {
            if (event.key == Key::Enter)
            {
                // Process the turn when Enter is pressed
                this->ProcessTurn_();
            }
            else if (event.key == Key::Escape)
            {
                this->m_gameState->SetShouldExit(true);
            }
        });
    }
}

void Engine::ProcessTurn_()
{
    m_gameState->on_turn_started.emit(m_gameState->GetMissionYear());
    m_turnProcessor->ProcessTurn(m_gameState->GetMissionYear(), m_gameState->GetNumFactions(), *m_gameState);
    m_gameState->IncrementMissionYear();
}

void Engine::Initialize_()
{
    std::cout << "Initializing game engine...\n";

    // Create EventBridge
    m_eventBridge = std::make_unique<EventBridge>(*m_gameState, *m_eventBus);

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

    m_popTypeRegistry = std::make_unique<PopTypeRegistry>();
    m_popTypeRegistry->Load("config/pop_types.json");

    m_luaRuntime = std::make_unique<LuaRuntime>();

    PopCompositionConfigParser compositionParser;
    m_popCompositionConfig     = std::make_unique<PopCompositionConfig>(compositionParser.ParseConfig("config/pop_composition.lua", *m_luaRuntime));
    m_popCompositionCalculator = std::make_unique<PopCompositionCalculator>(*m_popCompositionConfig, *m_luaRuntime);

    // Create test faction with a base
    auto pFaction = std::make_unique<Faction>();
    auto pBase = std::make_unique<Base>();
    pBase->SetFactionId(1);  // Test faction ID
    pBase->SetBaseId(1);     // Test base ID
    pBase->SetName("Test Base");

    std::cout << "Created test base with initial population: " << pBase->GetPopulation()->GetSize() << "\n";

    // Inject pop type registry and composition calculator into base population
    pBase->GetPopulation()->SetRegistry(m_popTypeRegistry.get());
    pBase->GetPopulation()->SetCompositionCalculator(m_popCompositionCalculator.get());

    // Wire base signals to EventBus
    m_eventBridge->WireBase(*pBase);

    // Add base to faction
    pFaction->AddBase(std::move(pBase));
    m_gameState->AddFaction(std::move(pFaction));

    std::cout << "Test setup complete. " << m_gameState->GetNumFactions() << " faction(s), "
              << m_gameState->GetFactions()[0]->GetBaseCount() << " base(s)\n";

    // Create population display and initialize with current population
    m_popDisplay = std::make_unique<PopulationDisplay>(*m_eventBus, *m_graphics);
    if (m_gameState->GetNumFactions() > 0 && m_gameState->GetFactions()[0]->GetBaseCount() > 0)
    {
        Base* pBase = m_gameState->GetFactions()[0]->GetBase(0);
        if (pBase && pBase->GetPopulation())
        {
            m_popDisplay->SetCurrentPop(pBase->GetPopulation()->GetSize());
            m_popDisplay->SetPopulation(pBase->GetPopulation());
        }
    }

    m_turnStageFactory->SetCompositionCalculator(m_popCompositionCalculator.get());
    m_turnStageFactory->LoadConfig("config/turn_stages.json");
    auto registry = m_turnStageFactory->CreateStages();
    TurnStageRepeatFlags_t repeatFlags;
    for (const auto& config : m_turnStageFactory->GetStageConfigs())
    {
        auto stageEnum = magic_enum::enum_cast<TurnStage>(config.id);
        if (stageEnum.has_value())
        {
            repeatFlags[stageEnum.value()] = config.repeat_for_each_faction;
        }
    }
    m_turnProcessor = std::make_unique<TurnProcessor>(std::move(registry), std::move(repeatFlags));
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

void Engine::Render_()
{
    m_graphics->DrawText("Mission Year: " + std::to_string(m_gameState->GetMissionYear()), 20.f, 20.f, 24);
    
    if (m_popDisplay)
    {
        m_popDisplay->Render(20.f, 50.f);
    }
    
    m_graphics->DrawText("Press Enter to continue or Esc to quit.", 20.f, 80.f, 20);
}

} // namespace ac
