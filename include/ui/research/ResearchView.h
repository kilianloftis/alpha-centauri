#pragma once

#include "ui/IGameView.h"
#include "game/faction/ResearchManager.h"
#include "input/Input.h"

namespace ac
{

class ResearchView : public IGameView
{
public:

    explicit ResearchView(const ResearchManager& rResearch, WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;
private:
    const ResearchManager& m_rResearch;
};

} // namespace ac
