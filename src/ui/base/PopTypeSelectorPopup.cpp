#include "ui/base/PopTypeSelectorPopup.h"
#include "game/Faction.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

PopTypeSelectorPopup::PopTypeSelectorPopup(
    const Faction& rFaction,
    WindowLayout_t layout,
    std::function<void(const PopTypeConfig&)> onPopTypeSelected
)
    : UIElement(layout)
    , m_rFaction(rFaction)
    , m_onPopTypeSelected(std::move(onPopTypeSelected))
{
}

std::vector<const PopTypeConfig*> PopTypeSelectorPopup::GetAvailablePopTypes_() const
{
    return m_rFaction.GetAvailablePopTypes();
}

void PopTypeSelectorPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }
    const float padding = kPaddingRatio * static_cast<float>(rGraphics.GetWindowWidth());

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * kHeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * kEntryFontSizeRatio);
    const float        lineHeight     = m_layout.height * kLineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 40, 230});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color::Yellow());

    rGraphics.DrawText("Select Pop Type", m_layout.x + padding, m_layout.y + padding, headerFontSize, Color::Yellow());

    const auto pAvailableTypes = GetAvailablePopTypes_();

    if (pAvailableTypes.empty())
    {
        rGraphics.DrawText("No pop types available", m_layout.x + padding, m_layout.y + lineHeight * 2.f, entryFontSize, Color::White());
        return;
    }

    float offsetY = lineHeight * 2.f;
    for (const PopTypeConfig* pConfig : pAvailableTypes)
    {
        rGraphics.DrawText(pConfig->name, m_layout.x + padding, m_layout.y + offsetY, entryFontSize, Color::White());
        offsetY += lineHeight;
    }
}

void PopTypeSelectorPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
    }
}

void PopTypeSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    const float lineHeight = m_layout.height * kLineHeightRatio;

    const float clickX = static_cast<float>(rEvent.x);
    const float clickY = static_cast<float>(rEvent.y);

    if (clickX < m_layout.x || clickX > m_layout.x + m_layout.width)
    {
        return;
    }

    const auto pAvailableTypes = GetAvailablePopTypes_();

    float offsetY = lineHeight * 2.f;
    for (const PopTypeConfig* pConfig : pAvailableTypes)
    {
        const float entryTop    = m_layout.y + offsetY;
        const float entryBottom = entryTop + lineHeight;

        if (clickY >= entryTop && clickY < entryBottom)
        {
            if (m_onPopTypeSelected)
            {
                m_onPopTypeSelected(*pConfig);
                m_bShouldClose = true;
            }
            return;
        }

        offsetY += lineHeight;
    }
}

} // namespace ac
