#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <vector>

namespace ac
{

class Graphics;
struct PopTypeConfig_t;

// Popup that lists a supplied set of pop types for the player to choose from.
class PopTypeSelectorPopup : public UIElement
{
public:
    PopTypeSelectorPopup(
        std::vector<const PopTypeConfig_t*> popTypes,
        WindowLayout_t layout,
        std::function<void(const PopTypeConfig_t&)> onPopTypeSelected
    );

    ~PopTypeSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::vector<const PopTypeConfig_t*> m_popTypes;
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(const PopTypeConfig_t&)> m_onPopTypeSelected;

    void CacheEntryRects_();
};

} // namespace ac
