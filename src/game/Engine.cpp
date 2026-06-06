#include "game/Engine.h"
#include "game/GameState.h"
#include "game/TurnStageFactory.h"
#include "game/TurnProcessor.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "input/KeyMapping.h"
#include <iostream>
#include <stdexcept>

namespace ac
{

Engine::Engine()
    : m_graphics(CreateGraphics())
    , m_input(CreateInput())
    , m_turnStageFactory(std::make_unique<TurnStageFactory>())
    , m_gameState(std::make_unique<GameState>())
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
        m_graphics->DrawText("Mission Year: " + std::to_string(m_gameState->GetMissionYear()), 20.f, 20.f, 24);
        m_graphics->DrawText("Press Enter to continue or Esc to quit.", 20.f, 80.f, 20);
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
    m_turnProcessor->ProcessTurn(m_gameState->GetMissionYear(), m_gameState->GetNumFactions());
    m_gameState->IncrementMissionYear();
}

void Engine::Initialize_()
{
    std::cout << "Initializing game engine...\n";
    m_turnStageFactory->LoadConfig("config/turn_stages.json");
    auto registry = m_turnStageFactory->CreateStages();
    m_turnProcessor = std::make_unique<TurnProcessor>(std::move(registry));
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
