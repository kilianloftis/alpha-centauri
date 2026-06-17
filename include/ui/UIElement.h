#pragma once

namespace ac
{

class Graphics;
struct KeyEvent_t;
struct MouseEvent_t;

struct Rectangle_t
{
    float x;
    float y;
    float width;
    float height;
};

using ResolvedLayout_t = Rectangle_t;
using RatioLayout_t = Rectangle_t;
using WindowLayout_t = Rectangle_t;

inline ResolvedLayout_t Resolve(const WindowLayout_t& windowLayout, const RatioLayout_t& ratioLayout)
{
    return {
            ratioLayout.x      * windowLayout.width,
            ratioLayout.y      * windowLayout.height,
            ratioLayout.width  * windowLayout.width,
            ratioLayout.height * windowLayout.height
        };
}

inline constexpr RatioLayout_t k_LeftPanelLayout  {0.25f, 0.25f, 0.0f,  0.75f};
inline constexpr RatioLayout_t k_CenterPanelLayout{0.5f,  0.25f, 0.25f, 0.75f};
inline constexpr RatioLayout_t k_RightPanelLayout {0.25f, 0.25f, 0.75f, 0.75f};
inline constexpr RatioLayout_t k_TopPanelLayout   {0.5f,  0.75f, 0.25f, 0.0f};
inline constexpr RatioLayout_t k_PopupLayout      {0.5f,  0.75f,  0.25f, 0.125f};

class UIElement
{
public:
    UIElement(ResolvedLayout_t layout, const Graphics& rGraphics)
        : m_layout(layout)
        , m_rGraphics(rGraphics)
    {}
    virtual ~UIElement() = default;

    virtual void Render() = 0;
    virtual void Update() = 0;

    bool Contains(float x, float y) const
    {
        return x >= m_layout.x && x < m_layout.x + m_layout.width && y >= m_layout.y && y < m_layout.y + m_layout.height;
    }

    virtual void HandleMouseClick(const MouseEvent_t& rEvent) {}
    virtual void HandleKey(const KeyEvent_t& rEvent) {}

    bool ShouldClose() const { return m_bShouldClose; }

protected:
    const ResolvedLayout_t m_layout;
    const Graphics& m_rGraphics;
    bool m_bShouldClose = false;
};

} // namespace ac
