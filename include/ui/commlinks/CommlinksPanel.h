#pragma once

#include "game/base/BaseTypes.h"
#include "ui/UIElement.h"

#include <functional>
#include <string>
#include <vector>

namespace ac
{

class Faction;
class GameState;
class StandingCaptions;
struct Color_t;

class CommlinksPanel : public UIElement
{
public:
    // onTalk receives the faction whose row was clicked.
    CommlinksPanel(GameState& rGameState, const StandingCaptions& rCaptions,
                   WindowLayout_t layout, std::function<void(FactionId_t)> onTalk);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void DrawFrame_(Graphics& rGraphics) const;
    void DrawIntegrity_(Graphics& rGraphics, FactionId_t playerId) const;
    void DrawColumnLabels_(Graphics& rGraphics) const;
    // One row per faction the player has met, in draw order.
    std::vector<const Faction*> MetFactions_(FactionId_t playerId) const;
    void DrawRows_(Graphics& rGraphics, FactionId_t playerId) const;
    void DrawRow_(Graphics& rGraphics, const Faction& rFaction, FactionId_t playerId,
                  int row) const;
    // padX and padY are fractions of the panel's width and height.
    void DrawAt_(Graphics& rGraphics, const std::string& rText, float padX, float padY,
                 unsigned int fontSize, const Color_t& color) const;

    GameState& m_rGameState;
    const StandingCaptions& m_rCaptions;
    std::function<void(FactionId_t)> m_onTalk;
};

} // namespace ac
