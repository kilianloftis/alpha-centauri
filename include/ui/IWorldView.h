#pragma once

#include "input/Input.h"
#include "ui/IGameView.h"

#include <optional>

namespace ac
{

// What UIManager needs from the persistent world view, beyond IGameView.
//
// The seam exists so UIManager can be built and tested without the whole concrete view tree:
// WorldView pulls in GameState, every registry, the map renderer and the input controllers, so
// naming it directly made UIManager untestable and put the manager's own rules (push/pop, the
// modal gate, when closed views are pruned) out of reach of any test.
//
// BlocksTurnAdvance is not here: IGameView already declares it virtual with a default.
class IWorldView : public IGameView
{
public:
    explicit IWorldView(WindowLayout_t layout)
        : IGameView(layout)
    {
    }

    // Edge scrolling. bEnabled is false while an overlay covers the map; mousePosition is empty
    // until the input backend has seen the pointer.
    virtual void UpdateCameraInput(bool bEnabled,
                                   std::optional<MousePosition_t> mousePosition) = 0;

    // Consumes a queued auto-end-turn request. Called between input and paint so a turn never
    // advances from the render path.
    virtual void ProcessPendingAutoEndTurn() = 0;
};

} // namespace ac
