#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"
#include <memory>
#include <vector>

namespace ac
{

class Graphics;

class IGameView
{
public:
    explicit IGameView(WindowLayout_t layout)
        : m_layout(layout)
    {}
    virtual ~IGameView() = default;

    virtual void Render(Graphics& rGraphics)
    {
        for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
        {
            if (m_elements[i]->ShouldClose())
            {
                m_elements.erase(m_elements.begin() + i);
            }
        }
        for (const auto& pElement : m_elements)
        {
            pElement->Render(rGraphics);
        }
    }

    virtual bool HandleKey(const KeyEvent_t& rEvent)
    {
        // An open modal element is the only thing that sees the key, and the key always reports
        // handled so global shortcuts cannot fire under it.
        if (UIElement* pModal = GetTopModalElement())
        {
            (void)pModal->HandleKey(rEvent);
            return true;
        }

        for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
        {
            if (m_elements[i]->HandleKey(rEvent))
            {
                return true;
            }
        }
        return false;
    }

    virtual void HandleMouse(const MouseEvent_t& rEvent)
    {
        if (!rEvent.bPressed)
        {
            return;
        }

        // Exclusive routing: a modal element captures every press, including outside its own
        // Contains rect, so outside-click dismiss works and underlying chrome/map cannot act.
        if (UIElement* pModal = GetTopModalElement())
        {
            pModal->HandleMouseClick(rEvent);
            return;
        }

        for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
        {
            if (m_elements[i]->Contains(static_cast<float>(rEvent.x), static_cast<float>(rEvent.y)))
            {
                m_elements[i]->HandleMouseClick(rEvent);
                break;
            }
        }
    }

    virtual void OnPushed(Graphics& /*rGraphics*/) {}
    virtual void OnPopped() {}

    bool ShouldClose() const { return m_bShouldClose; }

    // A topmost modal element that is not closing. Both it and an overlay gate turn advance.
    bool HasModalElement() const { return GetTopModalElement() != nullptr; }

    // Whether this view forbids TurnProcessor::Advance; any open modal element does.
    virtual bool BlocksTurnAdvance() const { return HasModalElement(); }

    // Per-frame logic that must run even when paint is skipped (timers, panel refresh).
    virtual void UpdateFrame() {}

    // True while this view needs a paint every frame (combat playback, etc.).
    virtual bool NeedsContinuousRedraw() const { return false; }

protected:
    // The topmost modal element not already closing, or nullptr; the one exclusive-capture rule.
    UIElement* GetTopModalElement() const
    {
        for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
        {
            if (m_elements[i]->IsModal() && !m_elements[i]->ShouldClose())
            {
                return m_elements[i].get();
            }
        }
        return nullptr;
    }

    // Close every open modal element before pushing a replacement (no stacking of the same
    // kind via an underlying opener that sits outside the popup chrome).
    void DismissOpenModals_()
    {
        for (auto& pElement : m_elements)
        {
            if (pElement->IsModal() && !pElement->ShouldClose())
            {
                pElement->RequestClose();
            }
        }
    }

    const WindowLayout_t m_layout;
    std::vector<std::unique_ptr<UIElement>> m_elements;
    bool m_bShouldClose = false;
};

} // namespace ac
