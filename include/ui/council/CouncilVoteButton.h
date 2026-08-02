#pragma once

#include "ui/UIElement.h"
#include "graphics/Graphics.h"
#include <functional>

namespace ac
{

class CouncilVoteButton : public UIElement
{
public:
    CouncilVoteButton(WindowLayout_t layout, std::function<void()> onVote);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::function<void()> m_onVote;
};

} // namespace ac
