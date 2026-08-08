#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <memory>
#include <string>

namespace ac
{

// TODO: SatelliteLabeledButton is a generic labelled button with a satellite-scoped name and
// style block. Moving it to ui/ would let this widget stop reaching into another screen's
// namespace; it needs its own style section first.
class SatelliteLabeledButton;

// A modal "here is what happened / why that did not work" popup: title, message, OK.
// Was OrbitalAttackOutcomePopup, which was already generic apart from a hardcoded header — the
// council gates needed exactly this widget, and a second copy is how the seven list-picker
// clones in package 14 started.
class NoticePopup : public UIElement
{
public:
    NoticePopup(WindowLayout_t layout,
                std::string title,
                std::string message,
                std::function<void()> onOk = {});
    // Out of line: m_pOkButton's type is only forward-declared here.
    ~NoticePopup() override;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void Close_();

    std::string m_title;
    std::string m_message;
    std::function<void()> m_onOk;
    std::unique_ptr<SatelliteLabeledButton> m_pOkButton;
};

} // namespace ac
