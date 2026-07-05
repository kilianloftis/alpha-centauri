#pragma once

#include "game/IConstructable.h"
#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <string>
#include <vector>

namespace ac
{

class Graphics;

// Popup that lists buildings available for construction.
// Clicking one sets it as the current production target.
class ProductionSelectorPopup : public UIElement
{
public:
    ProductionSelectorPopup(
        std::vector<const IConstructable*> availableItems,
        WindowLayout_t layout,
        std::function<void(const IConstructable&)> onItemSelected
    );

    ~ProductionSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::vector<const IConstructable*> m_availableItems;
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(const IConstructable&)> m_onItemSelected;

    void CacheEntryRects_();
};

} // namespace ac
