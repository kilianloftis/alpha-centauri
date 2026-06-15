#pragma once

namespace ac
{

class Graphics;

class IBasePanel
{
public:
    virtual ~IBasePanel() = default;
    virtual void Render(Graphics& rGraphics) = 0;
};

} // namespace ac
