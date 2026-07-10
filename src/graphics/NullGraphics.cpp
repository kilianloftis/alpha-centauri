#include "graphics/Graphics.h"
#include <iostream>
#include <memory>

namespace ac
{

namespace
{

class NullGraphics : public Graphics
{
public:
NullGraphics()
    {
        std::cout << "[Graphics] Null graphics backend selected. No rendering will occur.\n";
    }

void Clear() override
    {
    }

void Display() override
    {
    }

bool LoadTexture(const std::string& id, const std::string& path) override
    {
        std::cout << "[Graphics] Skipping loadTexture('" << id << "', '" << path << "') in null backend.\n";
        return false;
    }

bool DrawSprite(const std::string& textureId, float x, float y) override
    {
        std::cout << "[Graphics] Skipping drawSprite('" << textureId << "', " << x << ", " << y << ") in null backend.\n";
        return false;
    }

void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color_t& color = Color_t::White()) override
    {
        std::cout << "[Graphics] Skipping draw text: '" << text << "'\n";
    }

void DrawRect(float x, float y, float width, float height, const Color_t& color, float thickness) override
    {
    }

void DrawFilledRect(float x, float y, float width, float height, const Color_t& color) override
    {
    }

unsigned int GetWindowWidth() const override
    {
        return 1280;
    }

unsigned int GetWindowHeight() const override
    {
        return 900;
    }
};

} // namespace

std::unique_ptr<Graphics> CreateGraphics()
{
    return std::make_unique<NullGraphics>();
}

} // namespace ac
