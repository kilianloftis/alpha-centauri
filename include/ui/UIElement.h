#pragma once

#include <string>

namespace ac
{

class Graphics;

class UIElement
{
public:
    virtual ~UIElement() = default;

    virtual void Draw(Graphics& rGraphics) = 0;
    virtual void Update(float deltaTime) = 0;

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
