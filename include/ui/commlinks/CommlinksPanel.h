#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;

class CommlinksPanel : public UIElement
{
public:
    CommlinksPanel(GameState& rGameState, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    GameState& m_rGameState;
};

} // namespace ac
