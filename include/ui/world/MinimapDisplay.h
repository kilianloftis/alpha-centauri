#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;
class Graphics;

// Full-map terrain overview in the world dashboard right panel. Same elevation /
// fog / shroud colours as WorldDisplay, without per-tile labels or overlays.
class MinimapDisplay : public UIElement
{
public:
    MinimapDisplay(const GameState& rGameState, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    const GameState& m_rGameState;
};

} // namespace ac
