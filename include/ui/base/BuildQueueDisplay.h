#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class BaseManager;
class Graphics;

// Left-column queue panel. The queue list itself is still empty; the Hurry button at the
// bottom is the first live control. Grey when the queued item cannot be hurried.
class BuildQueueDisplay : public UIElement
{
public:
    BuildQueueDisplay(
        const BaseManager& rBase,
        WindowLayout_t layout,
        std::function<void()> onHurryClicked = nullptr
    );
    ~BuildQueueDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    WindowLayout_t HurryButtonLayout_() const;
    bool HurryEnabled_() const;

    std::function<void()> m_onHurryClicked;
    const BaseManager& m_rBase;
};

} // namespace ac
