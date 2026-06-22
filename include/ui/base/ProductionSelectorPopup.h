#pragma once

#include "game/buildings/BuildingConfigParser.h"
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
        std::vector<const BuildingConfig_t*> availableBuildings,
        WindowLayout_t layout,
        std::function<void(const BuildingConfig_t&)> onBuildingSelected
    );

    ~ProductionSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    static constexpr float k_HeaderFontSizeRatio = 0.04f;
    static constexpr float k_EntryFontSizeRatio  = 0.03f;
    static constexpr float k_LineHeightRatio      = 0.05f;
    static constexpr float k_PaddingRatio         = 0.02f;

    std::vector<const BuildingConfig_t*> m_availableBuildings;
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(const BuildingConfig_t&)> m_onBuildingSelected;

    void CacheEntryRects_();
};

} // namespace ac
