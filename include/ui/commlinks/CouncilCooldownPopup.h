#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <optional>

namespace ac
{

// Shown when the player opens the Planetary Council while still on propose cooldown.
class CouncilCooldownPopup : public UIElement
{
public:
    CouncilCooldownPopup(
        WindowLayout_t layout,
        int memberCooldownYears,
        int governorCooldownYears,
        int playerCooldownYears,
        std::optional<int> lastProposedYear,
        int yearsRemaining,
        std::function<void()> onOk
    );

    ~CouncilCooldownPopup() override = default;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void CacheOkButtonRect_();
    void Close_();

    int m_memberCooldownYears = 0;
    int m_governorCooldownYears = 0;
    int m_playerCooldownYears = 0;
    std::optional<int> m_lastProposedYear;
    int m_yearsRemaining = 0;
    std::function<void()> m_onOk;
    Rectangle_t m_okButtonRect{};
};

} // namespace ac
