#pragma once

namespace ac
{

class Graphics;
struct KeyEvent_t;
struct MouseEvent_t;

struct ResolvedLayout_t
{
    float x;
    float y;
    float width;
    float height;
};

struct PanelLayout_t
{
    float widthRatio;
    float heightRatio;
    float xRatio;
    float yRatio;

    ResolvedLayout_t Resolve(float windowWidth, float windowHeight) const
    {
        return {
            xRatio      * windowWidth,
            yRatio      * windowHeight,
            widthRatio  * windowWidth,
            heightRatio * windowHeight
        };
    }
};

inline constexpr PanelLayout_t kLeftPanelLayout  {0.25f, 0.25f, 0.0f,  0.75f};
inline constexpr PanelLayout_t kCenterPanelLayout{0.5f,  0.25f, 0.25f, 0.75f};
inline constexpr PanelLayout_t kRightPanelLayout {0.25f, 0.25f, 0.75f, 0.75f};
inline constexpr PanelLayout_t kTopPanelLayout   {0.5f,  0.75f, 0.25f, 0.0f};

class UIElement
{
public:
    virtual ~UIElement() = default;

    virtual void Render(Graphics& rGraphics) = 0;
    virtual void Update() = 0;

    bool Contains(float x, float y) const
    {
        return x >= m_x && x < m_x + m_width && y >= m_y && y < m_y + m_height;
    }

    virtual void HandleMouseClick(const MouseEvent_t& rEvent) {}

    bool ShouldClose() const { return m_bShouldClose; }

protected:
    float m_x = 0.f;
    float m_y = 0.f;
    float m_width = 0.f;
    float m_height = 0.f;
    bool m_bShouldClose = false;
};

} // namespace ac
