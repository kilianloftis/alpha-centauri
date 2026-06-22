#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <vector>

namespace ac
{

class Graphics;
struct PopTypeConfig;

// Popup that lists a supplied set of pop types for the player to choose from.
class PopTypeSelectorPopup : public UIElement
{
public:
    PopTypeSelectorPopup(
        std::vector<const PopTypeConfig*> popTypes,
        WindowLayout_t layout,
        std::function<void(const PopTypeConfig&)> onPopTypeSelected
    );

    ~PopTypeSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    static constexpr float kHeaderFontSizeRatio = 0.04f;
    static constexpr float kEntryFontSizeRatio  = 0.03f;
    static constexpr float kLineHeightRatio     = 0.05f;
    static constexpr float kPaddingRatio        = 0.02f;

    std::vector<const PopTypeConfig*> m_popTypes;
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(const PopTypeConfig&)> m_onPopTypeSelected;

    void CacheEntryRects_();
};

} // namespace ac
