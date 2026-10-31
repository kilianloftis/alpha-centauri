#pragma once

#include "game/base/BaseTypes.h"
#include "ui/UIElement.h"

#include <string>

namespace ac
{

class Faction;
class GameState;
class StandingCaptions;
struct Color_t;

class CommlinksPanel : public UIElement
{
public:
    CommlinksPanel(GameState& rGameState, const StandingCaptions& rCaptions,
                   WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    void DrawFrame_(Graphics& rGraphics) const;
    void DrawIntegrity_(Graphics& rGraphics, FactionId_t playerId) const;
    void DrawColumnLabels_(Graphics& rGraphics) const;
    // One row per faction the player has met.
    void DrawRows_(Graphics& rGraphics, FactionId_t playerId) const;
    void DrawRow_(Graphics& rGraphics, const Faction& rFaction, FactionId_t playerId,
                  int row) const;
    // padX and padY are fractions of the panel's width and height.
    void DrawAt_(Graphics& rGraphics, const std::string& rText, float padX, float padY,
                 unsigned int fontSize, const Color_t& color) const;

    GameState& m_rGameState;
    const StandingCaptions& m_rCaptions;
};

} // namespace ac
