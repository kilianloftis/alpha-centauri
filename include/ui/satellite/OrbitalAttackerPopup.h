#pragma once

#include "game/orbital/OrbitalAttack.h"
#include "ui/UIElement.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ac
{

class SatelliteLabeledButton;

// Lists ready orbital attacker types. Selection is mutually exclusive. Attack confirms the
// chosen attacker; Cancel dismisses without acting.
class OrbitalAttackerPopup : public UIElement
{
public:
    OrbitalAttackerPopup(WindowLayout_t layout,
                         std::vector<OrbitalAttackerOption_t> attackers,
                         std::function<void(BuildingId_t)> onConfirm,
                         std::function<void()> onCancel = {});

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void RebuildButtons_();
    void SelectAttacker_(size_t index);
    void Confirm_();
    void Cancel_();

    std::vector<OrbitalAttackerOption_t> m_attackers;
    std::optional<size_t> m_selectedIndex;
    std::function<void(BuildingId_t)> m_onConfirm;
    std::function<void()> m_onCancel;

    std::vector<std::unique_ptr<SatelliteLabeledButton>> m_attackerButtons;
    std::unique_ptr<SatelliteLabeledButton> m_pAttackButton;
    std::unique_ptr<SatelliteLabeledButton> m_pCancelButton;
};

} // namespace ac
