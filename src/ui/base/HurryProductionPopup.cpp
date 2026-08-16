#include "ui/base/HurryProductionPopup.h"

#include "graphics/Graphics.h"
#include "input/Input.h"

#include <charconv>
#include <optional>
#include <stdexcept>
#include <system_error>

namespace ac
{

namespace
{

std::optional<char> DigitFromKey_(Key_t key)
{
    const int ordinal = static_cast<int>(key) - static_cast<int>(Key_t::Num0);
    if (ordinal < 0 || ordinal > 9)
    {
        return std::nullopt;
    }
    return static_cast<char>('0' + ordinal);
}

} // namespace

HurryProductionPopup::HurryProductionPopup(WindowLayout_t layout,
                                           int finishCreditCost,
                                           std::function<void(int)> onConfirm,
                                           const HurryProductionPopupStyle_t& rStyle)
    : UIElement(layout)
    , m_finishCreditCost(finishCreditCost)
    , m_creditsText(std::to_string(finishCreditCost))
    , m_cursor(m_creditsText.size())
    , m_onConfirm(std::move(onConfirm))
    , m_rStyle(rStyle)
{
    if (finishCreditCost <= 0)
    {
        throw std::invalid_argument("HurryProductionPopup: finish cost "
                                    + std::to_string(finishCreditCost) + " must be positive");
    }
    if (!m_onConfirm)
    {
        throw std::invalid_argument("HurryProductionPopup was given no confirm handler");
    }
}

WindowLayout_t HurryProductionPopup::FieldLayout_() const
{
    return ResolveLayout(m_layout, m_rStyle.fieldLayout);
}

WindowLayout_t HurryProductionPopup::OkLayout_() const
{
    return ResolveLayout(m_layout, m_rStyle.okButtonLayout);
}

WindowLayout_t HurryProductionPopup::CancelLayout_() const
{
    return ResolveLayout(m_layout, m_rStyle.cancelButtonLayout);
}

int HurryProductionPopup::ParsedCredits_() const
{
    if (m_creditsText.empty())
    {
        return 0;
    }
    int value = 0;
    const char* pBegin = m_creditsText.data();
    const char* pEnd = pBegin + m_creditsText.size();
    const auto [pParsed, err] = std::from_chars(pBegin, pEnd, value);
    if (err != std::errc{} || pParsed != pEnd || value < 0)
    {
        return 0;
    }
    return value;
}

void HurryProductionPopup::InsertDigit_(char digit)
{
    // Ten digits is past int range; the field is a spend amount, not a log.
    if (m_creditsText.size() >= 9)
    {
        return;
    }
    m_creditsText.insert(m_cursor, 1, digit);
    ++m_cursor;
}

void HurryProductionPopup::Confirm_()
{
    m_bShouldClose = true;
    const int credits = ParsedCredits_();
    if (credits > 0)
    {
        m_onConfirm(credits);
    }
}

void HurryProductionPopup::Cancel_()
{
    m_bShouldClose = true;
}

void HurryProductionPopup::DrawButton_(Graphics& rGraphics, const WindowLayout_t& rLayout,
                                       const std::string& rLabel) const
{
    rGraphics.DrawFilledRect(
        rLayout.x, rLayout.y, rLayout.width, rLayout.height, m_rStyle.buttonFillColor);
    rGraphics.DrawRect(
        rLayout.x, rLayout.y, rLayout.width, rLayout.height, m_rStyle.buttonBorderColor);
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * m_rStyle.buttonFontSizeRatio);
    rGraphics.DrawText(
        rLabel,
        rLayout.x + rLayout.width * m_rStyle.buttonTextPadXRatio,
        rLayout.y + rLayout.height * m_rStyle.buttonTextPadYRatio,
        fontSize,
        m_rStyle.buttonLabelColor);
}

void HurryProductionPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const float padding = m_rStyle.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * m_rStyle.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * m_rStyle.entryFontSizeRatio);

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, m_rStyle.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, m_rStyle.borderColor);

    rGraphics.DrawText(
        "Hurry", m_layout.x + padding, m_layout.y + padding, headerFontSize, m_rStyle.headerColor);

    const std::string message = "Finishing construction costs " + std::to_string(m_finishCreditCost)
                                + " credits.";
    rGraphics.DrawText(
        message,
        m_layout.x + padding,
        m_layout.y + padding + m_layout.height * m_rStyle.headerFontSizeRatio * 1.4f,
        entryFontSize,
        m_rStyle.messageColor);

    const WindowLayout_t field = FieldLayout_();
    rGraphics.DrawFilledRect(
        field.x, field.y, field.width, field.height, m_rStyle.fieldFillColor);
    rGraphics.DrawRect(
        field.x, field.y, field.width, field.height, m_rStyle.fieldBorderColor);
    rGraphics.DrawText(
        m_creditsText.substr(0, m_cursor) + "|" + m_creditsText.substr(m_cursor),
        field.x + field.width * 0.04f,
        field.y + field.height * 0.2f,
        entryFontSize,
        m_rStyle.fieldTextColor);

    DrawButton_(rGraphics, OkLayout_(), "OK");
    DrawButton_(rGraphics, CancelLayout_(), "Cancel");
}

bool HurryProductionPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        Cancel_();
        return true;
    }
    if (rEvent.key == Key_t::Enter)
    {
        Confirm_();
        return true;
    }
    if (rEvent.key == Key_t::Backspace)
    {
        if (m_cursor > 0)
        {
            m_creditsText.erase(m_cursor - 1, 1);
            --m_cursor;
        }
        return true;
    }
    if (rEvent.key == Key_t::ArrowLeft)
    {
        if (m_cursor > 0)
        {
            --m_cursor;
        }
        return true;
    }
    if (rEvent.key == Key_t::ArrowRight)
    {
        if (m_cursor < m_creditsText.size())
        {
            ++m_cursor;
        }
        return true;
    }
    if (const std::optional<char> digit = DigitFromKey_(rEvent.key))
    {
        InsertDigit_(*digit);
        return true;
    }
    return false;
}

void HurryProductionPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }
    if (ContainsMouseCoord(OkLayout_(), rEvent))
    {
        Confirm_();
        return;
    }
    if (ContainsMouseCoord(CancelLayout_(), rEvent))
    {
        Cancel_();
    }
}

} // namespace ac
