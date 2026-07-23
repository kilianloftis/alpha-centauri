#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;

// Shows the base name in the center-panel header slot.
class BaseNameDisplay : public UIElement
{
public:
    BaseNameDisplay(const BaseManager* pBase, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager* m_pBase = nullptr;
};

} // namespace ac
