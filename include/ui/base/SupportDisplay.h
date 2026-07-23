#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;

// Icons of units that claim this base as home (support).
class SupportDisplay : public UIElement
{
public:
    SupportDisplay(const BaseManager* pBase, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager* m_pBase = nullptr;
};

} // namespace ac
