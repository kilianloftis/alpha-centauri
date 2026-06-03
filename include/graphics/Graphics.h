#pragma once

#include <memory>
#include <string>

namespace ac {

class Graphics {
public:
    virtual ~Graphics() = default;

    virtual bool Initialize() = 0;
    virtual void Clear() = 0;
    virtual void Display() = 0;
    virtual bool LoadTexture(const std::string& id, const std::string& path) = 0;
    virtual bool DrawSprite(const std::string& textureId, float x, float y) = 0;
    virtual void DrawText(const std::string& text, float x, float y, unsigned int size = 24) = 0;
};

std::unique_ptr<Graphics> CreateGraphics();

} // namespace ac
