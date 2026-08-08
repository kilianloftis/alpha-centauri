#pragma once

#include "ui/UIElement.h"
#include "game/faction/ResearchManager.h"
#include <string>

namespace ac
{

class CurrentResearchPanel : public UIElement
{
public:
    CurrentResearchPanel(const ResearchManager& rResearch, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override {}

private:
    const ResearchManager& m_rResearch;
};

} // namespace ac
