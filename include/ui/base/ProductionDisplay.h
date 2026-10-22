#pragma once

#include "ui/UIElement.h"
#include <functional>
#include <string>

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
        std::function<void()> onClicked
    );
    ~ProductionDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void DrawHeader_(Graphics& rGraphics) const;
    void DrawEntry_(Graphics& rGraphics, const std::string& rText, float lineIndex) const;

    std::function<void()> m_onClicked;
    const BaseManager& m_rBase;
    const BaseDisplaySnapshot_t& m_rSnapshot;
    SpriteLibrary& m_rSprites;
};

} // namespace ac
