#pragma once

#include "ui/UIElement.h"
#include "game/units/UnitSlotConfig.h"
#include <functional>
#include <vector>

namespace ac
{

struct UnitComponentConfig_t;

class SlotColumnPanel : public UIElement
{
public:
    struct SlotEntry_t
    {
        const UnitSlotConfig_t* pSlotConfig;
        std::function<const UnitComponentConfig_t*()> getComponent;
        std::function<void()> onClicked;
    };

    SlotColumnPanel(std::vector<SlotEntry_t> slots, WindowLayout_t layout);
    ~SlotColumnPanel() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void CacheRects_();
    bool NeedsScroll_() const;

    std::vector<SlotEntry_t> m_slots;
    int m_scrollOffset = 0;

    Rectangle_t m_upArrowRect{};
    Rectangle_t m_downArrowRect{};
    std::vector<Rectangle_t> m_slotRects;

    static constexpr int   k_VisibleSlots      = 3;
    static constexpr float k_ArrowHeightRatio  = 0.08f;
    static constexpr float k_LabelFontSizeRatio = 0.08f;
    static constexpr float k_NameFontSizeRatio  = 0.07f;
    static constexpr float k_PaddingRatio       = 0.04f;
};

} // namespace ac
