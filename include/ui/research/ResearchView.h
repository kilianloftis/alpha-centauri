#pragma once

#include "ui/UIGroup.h"
#include "game/faction/ResearchManager.h"
#include "input/Input.h"

namespace ac
{

class ResearchView : public UIGroup
{
public:

    explicit ResearchView(ResearchManager* pResearch, WindowLayout_t layout);

    void HandleKey(const KeyEvent_t& rEvent) override;
private:
    ResearchManager* m_pResearch;
};

} // namespace ac
