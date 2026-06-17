#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

struct PanelLayout
{
    float widthRatio;
    float heightRatio;
    float xRatio;
    float yRatio;
};

// Predefined layouts for common panel positions
inline constexpr PanelLayout kLeftPanelLayout{0.25f, 0.25f, 0.0f, 0.75f};   // 1/4 width, bottom left
inline constexpr PanelLayout kCenterPanelLayout{0.5f, 0.25f, 0.25f, 0.75f}; // 1/2 width, bottom center
inline constexpr PanelLayout kRightPanelLayout{0.25f, 0.25f, 0.75f, 0.75f};  // 1/4 width, bottom right
inline constexpr PanelLayout kTopPanelLayout{0.5f, 0.75f, 0.25f, 0.0f};      // 1/2 width, full height, top

class UIPanel : public UIElement
{
public:
    explicit UIPanel(const PanelLayout& layout)
        : m_layout(layout)
    {
    }

    ~UIPanel() override = default;

    virtual void Draw(Graphics& rGraphics) override = 0;
    virtual void Update(float deltaTime) override {}
    virtual void HandleMouse(const MouseEvent_t& rEvent) {}

    const PanelLayout& GetLayout() const { return m_layout; }

    void UpdateLayout(const Graphics& rGraphics)
    {
        float windowWidth = static_cast<float>(rGraphics.GetWindowWidth());
        float windowHeight = static_cast<float>(rGraphics.GetWindowHeight());

        SetPosition(windowWidth * m_layout.xRatio, windowHeight * m_layout.yRatio);
        SetSize(windowWidth * m_layout.widthRatio, windowHeight * m_layout.heightRatio);
    }

    bool IsActive() const { return m_bIsActive; }
    void SetActive(bool bActive) { m_bIsActive = bActive; }

private:
    const PanelLayout m_layout;
    bool m_bIsActive = true;
};

} // namespace ac
