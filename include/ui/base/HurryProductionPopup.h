#pragma once

#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"

#include <cstddef>
#include <functional>
#include <string>

namespace ac
{

class Graphics;

// Modal: finish-cost copy, an editable credit amount (pre-filled with that cost), OK / Cancel.
class HurryProductionPopup : public UIElement
{
public:
    // Throws if onConfirm is empty: OK with nowhere to send the amount leaves the dialog
    // open with no feedback, the same trap ListSelectorPopup rejects at construction.
    HurryProductionPopup(WindowLayout_t layout,
                         int finishCreditCost,
                         std::function<void(int)> onConfirm,
                         const HurryProductionPopupStyle_t& rStyle);

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void Confirm_();
    void Cancel_();
    int ParsedCredits_() const;
    void InsertDigit_(char digit);
    WindowLayout_t FieldLayout_() const;
    WindowLayout_t OkLayout_() const;
    WindowLayout_t CancelLayout_() const;
    void DrawButton_(Graphics& rGraphics, const WindowLayout_t& rLayout,
                     const std::string& rLabel) const;

    int m_finishCreditCost = 0;
    std::string m_creditsText;
    // Insertion point in m_creditsText; drawn as '|'. Starts at the end so Backspace trims
    // the pre-filled quote the way a selected-to-end field would.
    size_t m_cursor = 0;
    std::function<void(int)> m_onConfirm;
    const HurryProductionPopupStyle_t& m_rStyle;
};

} // namespace ac
