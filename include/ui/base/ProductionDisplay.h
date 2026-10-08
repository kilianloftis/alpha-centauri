#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class BaseManager;
class Graphics;
class SpriteLibrary;
struct BaseDisplaySnapshot_t;

class ProductionDisplay : public UIElement
{
public:
    ProductionDisplay(
        const BaseManager& rBase,
        const BaseDisplaySnapshot_t& rSnapshot,
        SpriteLibrary& rSprites,
        WindowLayout_t layout,
        std::function<void()> onClicked = nullptr
    );
    ~ProductionDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::function<void()> m_onClicked;
    const BaseManager& m_rBase;
    const BaseDisplaySnapshot_t& m_rSnapshot;
    SpriteLibrary& m_rSprites;
};

} // namespace ac
