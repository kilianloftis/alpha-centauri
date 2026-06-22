#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class BaseManager;
class Graphics;

class ProductionDisplay : public UIElement
{
public:
    ProductionDisplay(
        const BaseManager* pBase,
        WindowLayout_t layout,
        std::function<void()> onClicked = nullptr
    );
    ~ProductionDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::function<void()> m_onClicked;
    const BaseManager* m_pBase = nullptr;

    static constexpr float k_HeaderFontSizeRatio = 0.04f;
    static constexpr float k_EntryFontSizeRatio  = 0.03f;
    static constexpr float k_LineHeightRatio      = 0.05f;
    static constexpr float k_LeftPaddingRatio     = 0.02f;
};

} // namespace ac
