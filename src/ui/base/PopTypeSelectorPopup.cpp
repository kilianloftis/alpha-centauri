#include "ui/base/PopTypeSelectorPopup.h"
#include "game/Faction.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

PopTypeSelectorPopup::PopTypeSelectorPopup(
    const Faction& rFaction,
    Graphics& rGraphics,
    PanelLayout_t layout,
    std::function<void(const PopTypeConfig&)> onPopTypeSelected
)
    : m_rFaction(rFaction)
    , m_rGraphics(rGraphics)
    , m_layout(layout)
    , m_onPopTypeSelected(std::move(onPopTypeSelected))
{
}

std::vector<const PopTypeConfig*> PopTypeSelectorPopup::GetAvailablePopTypes() const
{
    return m_rFaction.GetAvailablePopTypes();
}

void PopTypeSelectorPopup::Render(Graphics& rGraphics)
{
    if (!m_bVisible)
    {
        return;
    }

    const auto [x, y, width, height] = m_layout.Resolve(
        static_cast<float>(rGraphics.GetWindowWidth()),
        static_cast<float>(rGraphics.GetWindowHeight())
    );
    const float padding = kPaddingRatio * static_cast<float>(rGraphics.GetWindowWidth());

    const unsigned int headerFontSize = static_cast<unsigned int>(height * kHeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(height * kEntryFontSizeRatio);
    const float        lineHeight     = height * kLineHeightRatio;

    rGraphics.DrawFilledRect(x, y, width, height, Color{20, 20, 40, 230});
    rGraphics.DrawRect(x, y, width, height, Color::Yellow());

    rGraphics.DrawText("Select Pop Type", x + padding, y + padding, headerFontSize, Color::Yellow());

    const auto pAvailableTypes = GetAvailablePopTypes();

    if (pAvailableTypes.empty())
    {
        rGraphics.DrawText("No pop types available", x + padding, y + lineHeight * 2.f, entryFontSize, Color::White());
        return;
    }

    float offsetY = lineHeight * 2.f;
    for (const PopTypeConfig* pConfig : pAvailableTypes)
    {
        rGraphics.DrawText(pConfig->name, x + padding, y + offsetY, entryFontSize, Color::White());
        offsetY += lineHeight;
    }
}

void PopTypeSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (!m_bVisible || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    const auto [x, y, width, height] = m_layout.Resolve(
        static_cast<float>(m_rGraphics.GetWindowWidth()),
        static_cast<float>(m_rGraphics.GetWindowHeight())
    );
    const float lineHeight = height * kLineHeightRatio;

    const float clickX = static_cast<float>(rEvent.x);
    const float clickY = static_cast<float>(rEvent.y);

    if (clickX < x || clickX > x + width)
    {
        return;
    }

    const auto pAvailableTypes = GetAvailablePopTypes();

    float offsetY = lineHeight * 2.f;
    for (const PopTypeConfig* pConfig : pAvailableTypes)
    {
        const float entryTop    = y + offsetY;
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
