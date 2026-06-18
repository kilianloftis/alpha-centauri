#pragma once

#include "ui/UIElement.h"
#include "game/faction/ResearchManager.h"
#include <string>

namespace ac
{

inline constexpr RatioLayout_t k_CurrentResearchLabelLayout {0.0f, 0.0f,  1.0f, 0.35f};
inline constexpr RatioLayout_t k_CurrentResearchTargetLayout{0.0f, 0.35f, 1.0f, 0.4f};
inline constexpr RatioLayout_t k_CurrentResearchProgressLayout{0.0f, 0.75f, 1.0f, 0.25f};

class CurrentResearchPanel : public UIElement
{
public:
    CurrentResearchPanel(ResearchManager* pResearch, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    void Update() override;
    virtual void HandleMouseClick(const MouseEvent_t& rEvent) override {}

private:
    ResearchManager* m_pResearch = nullptr;
};

} // namespace ac
