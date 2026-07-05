#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;

class GrowthDisplay : public UIElement
{
public:
    GrowthDisplay(
        const BaseManager* pBase,
        WindowLayout_t layout
    );
    ~GrowthDisplay() override = default;

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager* m_pBase = nullptr;
};

} // namespace ac
