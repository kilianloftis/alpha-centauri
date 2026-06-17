#pragma once

#include "ui/UIPanel.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include <vector>
#include <memory>

namespace ac
{

class ResearchView;

class ResearchDisplay : public UIPanel
{
public:
    ResearchDisplay()
        : UIPanel(kTopPanelLayout)
    {
        SetActive(false);
    }

    ~ResearchDisplay() override = default;

    void SetResearchView(const ResearchView* pResearchView);

    void Draw(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
    void HandleMouse(const MouseEvent_t& rEvent);

private:
    std::vector<std::unique_ptr<UIPanel>> m_panels;
    const ResearchView* m_pResearchView = nullptr;
    void InitializePanels_();
    void UpdatePanelsResearchView_();
};

} // namespace ac
