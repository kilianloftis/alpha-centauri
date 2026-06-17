#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace ac
{

struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    static Color White() { return Color{255, 255, 255, 255}; }
    static Color Green() { return Color{0, 255, 0, 255}; }
    static Color Red() { return Color{255, 0, 0, 255}; }
    static Color Blue() { return Color{0, 0, 255, 255}; }
    static Color Yellow() { return Color{255, 255, 0, 255}; }
    static Color Black() { return Color{0, 0, 0, 255}; }
};

class Graphics
{
public:
    virtual ~Graphics() = default;

    virtual bool Initialize() = 0;
    virtual void Clear() = 0;
    virtual void Display() = 0;
    virtual bool LoadTexture(const std::string& id, const std::string& path) = 0;
    virtual bool DrawSprite(const std::string& textureId, float x, float y) = 0;
    virtual void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color& color = Color::White()) = 0;
    virtual void DrawRect(float x, float y, float width, float height, const Color& color, float thickness = 1.0f) = 0;
    virtual void DrawFilledRect(float x, float y, float width, float height, const Color& color) = 0;
    virtual unsigned int GetWindowWidth() const = 0;
    virtual unsigned int GetWindowHeight() const = 0;
};

std::unique_ptr<Graphics> CreateGraphics();

} // namespace ac
