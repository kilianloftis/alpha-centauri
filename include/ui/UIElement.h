#pragma once

#include <string>

namespace ac
{

class Graphics;
struct KeyEvent_t;
struct MouseEvent_t;

class UIElement
{
public:
    virtual ~UIElement() = default;

    virtual void Draw(Graphics& rGraphics) = 0;
    virtual void Update(float deltaTime) = 0;

    virtual bool HandleKey(const KeyEvent_t& rEvent) { return false; }
    virtual bool HandleMouse(const MouseEvent_t& rEvent) { return false; }

    bool Contains(float x, float y) const
    {
        return x >= m_x && x < m_x + m_width && y >= m_y && y < m_y + m_height;
    }

    bool IsVisible() const { return m_bVisible; }
    void SetVisible(bool bVisible) { m_bVisible = bVisible; }

    float GetX() const { return m_x; }
    float GetY() const { return m_y; }
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }

    void SetPosition(float x, float y) { m_x = x; m_y = y; }
    void SetSize(float width, float height) { m_width = width; m_height = height; }

protected:
    float m_x = 0.f;
    float m_y = 0.f;
    float m_width = 0.f;
    float m_height = 0.f;
    bool m_bVisible = true;
};

} // namespace ac
