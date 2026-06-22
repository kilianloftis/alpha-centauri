#pragma once

#include "input/Input.h"

#include <stdexcept>

namespace ac
{

class Graphics;
struct KeyEvent_t;

struct Rectangle_t
{
    float x;
    float y;
    float width;
    float height;
};

using RatioLayout_t = Rectangle_t;
using WindowLayout_t = Rectangle_t;

inline bool ContainsMouseCoord(const Rectangle_t& rRect, float x, float y)
{
    return x >= rRect.x && x < rRect.x + rRect.width
        && y >= rRect.y && y < rRect.y + rRect.height;
}

inline bool ContainsMouseCoord(const Rectangle_t& rRect, const MouseEvent_t& rEvent)
{
    return ContainsMouseCoord(rRect, static_cast<float>(rEvent.x), static_cast<float>(rEvent.y));
}

inline WindowLayout_t ResolveLayout(const WindowLayout_t& windowLayout, const RatioLayout_t& ratioLayout)
{
    if (ratioLayout.width > 1.0f || ratioLayout.height > 1.0f || ratioLayout.x > 1.0f || ratioLayout.y > 1.0f) {
        throw std::runtime_error("Invalid ratio layout");
    }
    return {
            windowLayout.x + ratioLayout.x * windowLayout.width,
            windowLayout.y + ratioLayout.y * windowLayout.height,
            ratioLayout.width  * windowLayout.width,
            ratioLayout.height * windowLayout.height
        };
}

inline constexpr RatioLayout_t k_FullscreenLayout {0.0f,  0.0f,  1.0f,  1.0f};
inline constexpr RatioLayout_t k_LeftPanelLayout  {0.0f,  0.25f, 0.25f, 0.75f};
inline constexpr RatioLayout_t k_TopPanelLayout   {0.2f, 0.1f, 0.6f, 0.6f};
inline constexpr RatioLayout_t k_CenterPanelLayout{0.2f, 0.7f, 0.6f, 0.3f};
inline constexpr RatioLayout_t k_RightPanelLayout {0.25f, 0.25f, 0.75f, 0.75f};
inline constexpr RatioLayout_t k_PopupLayout      {0.5f,  0.75f,  0.25f, 0.125f};
inline constexpr RatioLayout_t k_PopupLayoutSmall {0.3f, 0.3f, 0.4f, 0.4f};


class UIElement
{
public:
    explicit UIElement(WindowLayout_t layout)
        : m_layout(layout)
    {}
    virtual ~UIElement() = default;

    virtual void Render(Graphics& rGraphics) = 0;

    bool Contains(float x, float y) const
    {
        return x >= m_layout.x && x < m_layout.x + m_layout.width
            && y >= m_layout.y && y < m_layout.y + m_layout.height;
    }

    virtual void HandleMouseClick(const MouseEvent_t& rEvent) {}
    virtual bool HandleKey(const KeyEvent_t& rEvent) { return false; }

    bool ShouldClose() const { return m_bShouldClose; }

protected:
    const WindowLayout_t m_layout;
    bool m_bShouldClose = false;
};

} // namespace ac
