#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace ac
{

struct Color_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    static Color_t White() { return Color_t{255, 255, 255, 255}; }
    static Color_t Green() { return Color_t{0, 255, 0, 255}; }
    static Color_t Red() { return Color_t{255, 0, 0, 255}; }
    static Color_t Blue() { return Color_t{0, 0, 255, 255}; }
    static Color_t Yellow() { return Color_t{255, 255, 0, 255}; }
    static Color_t Black() { return Color_t{0, 0, 0, 255}; }
};

class Graphics
{
public:
    virtual ~Graphics() = default;

    virtual void Clear() = 0;
    virtual void Display() = 0;
    virtual bool LoadTexture(const std::string& id, const std::string& path) = 0;
    virtual bool DrawSprite(const std::string& textureId, float x, float y) = 0;
    virtual void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color_t& color = Color_t::White()) = 0;
    virtual void DrawRect(float x, float y, float width, float height, const Color_t& color, float thickness = 1.0f) = 0;
    virtual void DrawFilledRect(float x, float y, float width, float height, const Color_t& color) = 0;
    virtual unsigned int GetWindowWidth() const = 0;
    virtual unsigned int GetWindowHeight() const = 0;
};

std::unique_ptr<Graphics> CreateGraphics();

} // namespace ac
