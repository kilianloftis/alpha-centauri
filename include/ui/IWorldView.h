#pragma once

#include "input/Input.h"
#include "ui/IGameView.h"

#include <optional>

namespace ac
{

// What UIManager needs from the world view, so UIManager can be tested without the concrete
// view tree.
class IWorldView : public IGameView
{
public:
    explicit IWorldView(WindowLayout_t layout)
        : IGameView(layout)
    {
    }

    // Edge scrolling. bEnabled is false while an overlay covers the map; mousePosition is empty
    // until the input backend has seen the pointer. Returns true when the camera moved.
    virtual bool UpdateCameraInput(bool bEnabled,
                                   std::optional<MousePosition_t> mousePosition) = 0;

    // Auto-select, dashboard panels, path preview wiring. Runs every frame between input and
    // paint so skipping Render cannot stall selection / auto-end-turn arming.
    virtual void UpdatePresentation() = 0;

    // True after UpdatePresentation (or input) changed something the next paint must show.
    virtual bool ConsumePresentationDirty() = 0;

    // Consumes a queued auto-end-turn request. Called between input and paint so a turn never
    // advances from the render path.
    virtual void ProcessPendingAutoEndTurn() = 0;
};

} // namespace ac
