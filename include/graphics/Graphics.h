#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class PlatformEventQueue;

// Presentation knobs, shared by every backend so they cannot disagree about window size.
struct GraphicsConfig_t
{
    unsigned int windowWidth = 1280;
    unsigned int windowHeight = 900;
    std::string windowTitle = "Alpha Centauri";
    unsigned int framerateLimit = 60;
    // Tried in order; the first that opens wins. Empty is a config error, not "no text".
    std::vector<std::string> fontPaths = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
};

struct Color_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    static Color_t White()
    {
        return Color_t{255, 255, 255, 255};
    }
    static Color_t Green()
    {
        return Color_t{0, 255, 0, 255};
    }
    static Color_t Red()
    {
        return Color_t{255, 0, 0, 255};
    }
    static Color_t Blue()
    {
        return Color_t{0, 0, 255, 255};
    }
    static Color_t Yellow()
    {
        return Color_t{255, 255, 0, 255};
    }
    static Color_t Black()
    {
        return Color_t{0, 0, 0, 255};
    }
};

class Graphics
{
public:
    virtual ~Graphics() = default;

    // Drain the window's event queue into the PlatformEventQueue this backend was created with.
    // Separate from Display so rendering has no I/O side effects.
    virtual void PumpEvents() = 0;

    virtual void Clear() = 0;
    virtual void Display() = 0;
    virtual bool LoadTexture(const std::string& id, const std::string& path) = 0;
    virtual bool DrawSprite(const std::string& textureId, float x, float y) = 0;
    virtual void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color_t& color = Color_t::White()) = 0;
    virtual void DrawRect(float x, float y, float width, float height, const Color_t& color, float thickness = 1.0f) = 0;
    virtual void DrawFilledRect(float x, float y, float width, float height, const Color_t& color) = 0;
    virtual void DrawLine(float x1, float y1, float x2, float y2, const Color_t& color, float thickness = 1.0f) = 0;
    virtual unsigned int GetWindowWidth() const = 0;
    virtual unsigned int GetWindowHeight() const = 0;
};

// rEvents receives whatever this backend's window produces; the composition root owns it and
// passes the same one to CreateInput. Throws if the backend cannot reach a usable state.
std::unique_ptr<Graphics> CreateGraphics(PlatformEventQueue& rEvents,
                                         const GraphicsConfig_t& rConfig = {});

} // namespace ac
