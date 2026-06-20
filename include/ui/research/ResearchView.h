#pragma once

#include "ui/IGameView.h"
#include "game/faction/ResearchManager.h"
#include "input/Input.h"

namespace ac
{

class ResearchView : public IGameView
{
public:

    explicit ResearchView(ResearchManager* pResearch, WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;
private:
    ResearchManager* m_pResearch;

    static constexpr RatioLayout_t k_CurrentResearchLayout{0.0f, 0.0f, 1.0f, 1.0f};
};

} // namespace ac
