#pragma once

#include "ui/UIPanel.h"
#include "ui/research/ResearchView.h"
#include <string>

namespace ac
{

class CurrentResearchPanel : public UIPanel
{
public:
    CurrentResearchPanel()
        : UIPanel(kCenterPanelLayout)
    {
        SetPosition(20.f, 50.f);
        SetSize(560.f, 60.f);
    }

    void SetResearchView(const ResearchView* pResearchView) { m_pResearchView = pResearchView; }

    void Draw(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    const ResearchView* m_pResearchView = nullptr;
};

} // namespace ac
