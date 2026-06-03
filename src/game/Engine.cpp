#include "game/Engine.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "input/KeyMapping.h"
#include <iostream>
#include <stdexcept>

namespace ac {

Engine::Engine()
    : m_graphics(CreateGraphics())
    , m_input(CreateInput())
    {}

Engine::~Engine() = default;

void Engine::Run()
{
    Initialize_();
    PrintWelcome_();

    std::cout << "Graphics backend initialized successfully.\n";

    std::string lastKey = "";
    bool bShouldExit = false;

    while (!bShouldExit) {
        m_graphics->Clear();
        m_graphics->DrawText("Press a key inside the window. Enter will close it.", 20.f, 20.f, 24);
        if (!lastKey.empty()) {
            m_graphics->DrawText(lastKey, 20.f, 80.f, 48);
        }
        m_graphics->Display();

        if (auto key = m_input->CaptureKey()) {
            if (*key == Key::Enter) {
                bShouldExit = true;
            } else {
                lastKey = KeyToString(*key);
            }
        }
    }

    std::cout << "Enter pressed, closing the window.\n";
}

void Engine::CheckInitialized_() const
{
    if (!m_graphics) {
        std::cout << "No graphics backend available\n";
        throw std::runtime_error("Failed to create graphics backend");
    }
    if (!m_input) {
        std::cout << "No input backend available\n";
        throw std::runtime_error("Failed to create input backend");
    }
}

void Engine::Initialize_() const
{
    CheckInitialized_();
    std::cout << "Initializing game engine...\n";
}

void Engine::PrintWelcome_() const
{
    std::cout << "Welcome to Alpha Centauri (C++ rebuild)!\n";
}

} // namespace ac
