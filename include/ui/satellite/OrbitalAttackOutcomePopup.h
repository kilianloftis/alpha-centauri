#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <memory>
#include <string>

namespace ac
{

class SatelliteLabeledButton;

// Small dismissible popup describing the result of an orbital attack attempt.
class OrbitalAttackOutcomePopup : public UIElement
{
public:
    OrbitalAttackOutcomePopup(WindowLayout_t layout,
                              std::string message,
                              std::function<void()> onOk = {});

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void Close_();

    std::string m_message;
    std::function<void()> m_onOk;
    std::unique_ptr<SatelliteLabeledButton> m_pOkButton;
};

} // namespace ac
