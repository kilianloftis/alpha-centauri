#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;

class CommerceDisplay : public UIElement
{
public:
    CommerceDisplay(BaseManager& rBase, WindowLayout_t layout);
    ~CommerceDisplay() override = default;

    void Render(Graphics& rGraphics) override;

private:
    BaseManager& m_rBase;
};

} // namespace ac
