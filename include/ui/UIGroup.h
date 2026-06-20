#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"
#include <memory>
#include <vector>

namespace ac
{

class Graphics;

class UIGroup
{
public:
    explicit UIGroup(WindowLayout_t layout)
        : m_layout(layout)
    {}
    virtual ~UIGroup() = default;

    virtual void Render(Graphics& rGraphics)
    {
        for (const auto& pElement : m_elements)
        {
            pElement->Render(rGraphics);
        }
    }

    virtual void HandleKey(const KeyEvent_t& rEvent)
    {
        for (const auto& pElement : m_elements)
        {
            pElement->HandleKey(rEvent);
        }
    }

    virtual void HandleMouse(const MouseEvent_t& rEvent)
    {
        for (const auto& pElement : m_elements)
        {
            if (pElement->Contains(static_cast<float>(rEvent.x), static_cast<float>(rEvent.y)))
            {
                pElement->HandleMouseClick(rEvent);
                break;
            }
        }
    }

    virtual void Update() {}
    virtual void OnPushed(Graphics& /*rGraphics*/) {}
    virtual void OnPopped() {}

    bool ShouldClose() const { return m_bShouldClose; }

protected:
    const WindowLayout_t m_layout;
    std::vector<std::unique_ptr<UIElement>> m_elements;
    bool m_bShouldClose = false;
};

} // namespace ac
