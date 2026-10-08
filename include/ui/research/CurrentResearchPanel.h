#pragma once

#include "ui/UIElement.h"
#include "game/faction/ResearchManager.h"
#include <string>

namespace ac
{

class SpriteLibrary;
class TechRegistry;

class CurrentResearchPanel : public UIElement
{
public:
    CurrentResearchPanel(const ResearchManager& rResearch,
                         const TechRegistry& rTechs,
                         SpriteLibrary& rSprites,
                         WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override {}

private:
    const ResearchManager& m_rResearch;
    const TechRegistry& m_rTechs;
    SpriteLibrary& m_rSprites;
};

} // namespace ac
