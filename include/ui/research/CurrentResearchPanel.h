#pragma once

#include "ui/UIElement.h"
#include "game/faction/ResearchManager.h"
#include <string>

namespace ac
{

class CurrentResearchPanel : public UIElement
{
public:
    CurrentResearchPanel(ResearchManager* pResearch, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override {}

private:
    ResearchManager* m_pResearch = nullptr;
};

} // namespace ac
