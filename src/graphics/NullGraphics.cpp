#include "graphics/Graphics.h"
#include "input/PlatformEventQueue.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace ac
{

namespace
{

// A substitutable no-op, not a failing stub: loads and draws report success, because a caller
// should not have to special-case headless to distinguish "did nothing" from "went wrong".
class NullGraphics : public Graphics
{
public:
    explicit NullGraphics(const GraphicsConfig_t& rConfig)
        : m_config(rConfig)
    {
        std::cout << "[Graphics] Null graphics backend selected. No rendering will occur.\n";
    }

    void PumpEvents() override
    {
    }

    void Clear() override
    {
    }

    // Paces the frame loop. SFML's setFramerateLimit is the only thing that stops the loop
    // spinning, and a headless build has no window to provide it - without this a headless run
    // pins a core for as long as it lives.
    void Display() override
    {
        if (m_config.framerateLimit == 0)
        {
            return;
        }
        const auto frame = std::chrono::microseconds(1000000 / m_config.framerateLimit);
        const auto now = std::chrono::steady_clock::now();
        const auto nextFrame = m_lastDisplay + frame;
        if (now < nextFrame)
        {
            std::this_thread::sleep_for(nextFrame - now);
        }
        m_lastDisplay = std::chrono::steady_clock::now();
    }

    bool LoadTexture(const std::string&, const std::string&) override
    {
        return true;
    }

    bool DrawSprite(const std::string&, float, float) override
    {
        return true;
    }

    void DrawText(const std::string&, float, float, unsigned int, const Color_t&) override
    {
    }

    void DrawRect(float, float, float, float, const Color_t&, float) override
    {
    }

    void DrawFilledRect(float, float, float, float, const Color_t&) override
    {
    }

    void DrawLine(float, float, float, float, const Color_t&, float) override
    {
    }

    unsigned int GetWindowWidth() const override
    {
        return m_config.windowWidth;
    }

    unsigned int GetWindowHeight() const override
    {
        return m_config.windowHeight;
    }

private:
    GraphicsConfig_t m_config;
    std::chrono::steady_clock::time_point m_lastDisplay = std::chrono::steady_clock::now();
};

} // namespace

std::unique_ptr<Graphics> CreateGraphics(PlatformEventQueue&, const GraphicsConfig_t& rConfig)
{
    return std::make_unique<NullGraphics>(rConfig);
}

} // namespace ac
