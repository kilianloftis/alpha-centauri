#pragma once

#include "ui/UIElement.h"
#include <functional>
#include <vector>

namespace ac
{

struct UnitComponentConfig_t;

class ComponentSelectorPopup : public UIElement
{
public:
    ComponentSelectorPopup(
        std::vector<const UnitComponentConfig_t*> components,
        WindowLayout_t layout,
        std::function<void(const UnitComponentConfig_t&)> onSelected
    );
    ~ComponentSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void CacheEntryRects_();

    std::vector<const UnitComponentConfig_t*> m_components;
    std::function<void(const UnitComponentConfig_t&)> m_onSelected;
    std::vector<Rectangle_t> m_entryRects;

    static constexpr float k_TitleFontSizeRatio = 0.05f;
    static constexpr float k_EntryFontSizeRatio = 0.04f;
    static constexpr float k_EntryHeightRatio   = 0.07f;
    static constexpr float k_PaddingRatio        = 0.03f;
};

} // namespace ac
