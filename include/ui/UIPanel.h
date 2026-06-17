#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

class UIPanel : public UIElement
{
public:
    explicit UIPanel(const PanelLayout_t& layout)
        : m_layout(layout)
    {}

    ~UIPanel() override = default;

    virtual void Render(Graphics& rGraphics) override = 0;
    virtual void Update() override {}
    virtual void HandleMouse(const MouseEvent_t& rEvent) {}
protected:
    const PanelLayout_t m_layout;
};

} // namespace ac
