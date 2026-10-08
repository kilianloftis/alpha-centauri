#pragma once

#include "ui/IGameView.h"
#include "game/faction/ResearchManager.h"
#include "input/Input.h"

namespace ac
{

class SpriteLibrary;
class TechRegistry;

class ResearchView : public IGameView
{
public:

    ResearchView(const ResearchManager& rResearch,
                 const TechRegistry& rTechs,
                 SpriteLibrary& rSprites,
                 WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;
private:
    const ResearchManager& m_rResearch;
};

} // namespace ac
