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
};

} // namespace ac
