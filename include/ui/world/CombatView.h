#pragma once

#include "game/units/CombatResolver.h"
#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include "ui/world/CombatPresentation.h"

#include <functional>
#include <string>

namespace ac
{

class WorldDisplay;
class InfoPanelElement;

// Overlay shown while CombatPresentation plays back a resolved fight. Covers the world
// dashboard with combat panels, swallows all input, and closes itself when playback ends.
// WorldView keeps rendering the map underneath.
class CombatView : public IGameView
{
public:
    CombatView(WindowLayout_t layout,
               CombatResult_t result,
               const Tile& rAttackerTile,
               const Tile& rDefenderTile,
               std::string attackerName,
               std::string defenderName,
               WorldDisplay& rWorldDisplay,
               WindowLayout_t mapLayout,
               std::function<void()> onFinished);

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;
    void OnPopped() override;

private:
    void RefreshPanels_();
    void FinishIfDone_();

    WorldDisplay& m_rWorldDisplay;
    WindowLayout_t m_mapLayout;
    std::function<void()> m_onFinished;
    std::string m_attackerName;
    std::string m_defenderName;
    CombatPresentation m_presentation;

    InfoPanelElement* m_pAttackerPanel = nullptr;
    InfoPanelElement* m_pRoundPanel = nullptr;
    InfoPanelElement* m_pDefenderPanel = nullptr;
    bool m_bFinishedNotified = false;
};

} // namespace ac
