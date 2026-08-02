#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <string>

namespace ac
{

class SatelliteLabeledButton : public UIElement
{
public:
    SatelliteLabeledButton(WindowLayout_t layout,
                           std::string label,
                           std::function<void()> onClick,
                           bool bSelected = false);

    void SetSelected(bool bSelected);
    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::string m_label;
    std::function<void()> m_onClick;
    bool m_bSelected = false;
};

} // namespace ac
