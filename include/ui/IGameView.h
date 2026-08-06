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
        // Exclusive routing: while a modal element is open it is the only thing that may see
        // the key. Always report handled so UIManager does not fall through to global view
        // shortcuts under an in-view modal (WorldView has no overlay suppression).
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

    // True while this view has a topmost element reporting IsModal() that is not already
    // closing. Overlay = UIManager stack entry; in-view modal = this. Both gate turn advance
    // (see UIManager::CanAdvanceTurn).
    bool HasModalElement() const { return GetTopModalElement() != nullptr; }

    // Whether this view currently forbids TurnProcessor::Advance (Package 1's yield/resume
    // contract only says "the UI decides when to resume interaction"; this is that decision).
    // Default: any open modal element blocks advance. Views with additional non-element
    // reasons to block (none today) may override.
    virtual bool BlocksTurnAdvance() const { return HasModalElement(); }

protected:
    // Topmost element (z-order, checked back-to-front) reporting IsModal() and not already
    // closing, or nullptr. Shared by the default HandleKey/HandleMouse above and by views
    // (e.g. WorldView) that override both and must apply the identical exclusive-capture rule
    // rather than reinventing it.
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
