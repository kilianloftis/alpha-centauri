#include "ui/base/PopTypeSelectorPopup.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

namespace
{

constexpr float k_HeaderFontSizeRatio  = 0.04f;
constexpr float k_EntryFontSizeRatio   = 0.03f;
constexpr float k_LineHeightRatio      = 0.05f;
constexpr float k_PaddingRatio         = 0.02f;
constexpr float k_HeaderLineOffset     = 2.0f;
constexpr Color k_BackgroundColor      {20, 20, 40, 230};

} // namespace

PopTypeSelectorPopup::PopTypeSelectorPopup(
    std::vector<const PopTypeConfig_t*> popTypes,
    WindowLayout_t layout,
    std::function<void(const PopTypeConfig_t&)> onPopTypeSelected
)
    : UIElement(layout)
    , m_popTypes(std::move(popTypes))
    , m_onPopTypeSelected(std::move(onPopTypeSelected))
{
    CacheEntryRects_();
}

void PopTypeSelectorPopup::CacheEntryRects_()
{
    const float lineHeight = m_layout.height * k_LineHeightRatio;

    float offsetY = lineHeight * k_HeaderLineOffset;
    for (size_t i = 0; i < m_popTypes.size(); ++i)
    {
        m_entryRects.push_back(Rectangle_t{
            m_layout.x,
            m_layout.y + offsetY,
            m_layout.width,
            lineHeight
        });
        offsetY += lineHeight;
    }
}

void PopTypeSelectorPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }
    const float padding = k_PaddingRatio * m_layout.width;

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);
    const float        lineHeight     = m_layout.height * k_LineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color::Yellow());

    rGraphics.DrawText("Select Pop Type", m_layout.x + padding, m_layout.y + padding, headerFontSize, Color::Yellow());

    if (m_popTypes.empty())
    {
        rGraphics.DrawText(
            "No pop types available",
            m_layout.x + padding,
            m_layout.y + lineHeight * k_HeaderLineOffset,
            entryFontSize,
            Color::White()
        );
        return;
    }

    for (size_t i = 0; i < m_popTypes.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_popTypes[i]->name, rect.x + padding, rect.y, entryFontSize, Color::White());
    }
}

bool PopTypeSelectorPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void PopTypeSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (!ContainsMouseCoord(m_layout, rEvent))
    {
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        if (ContainsMouseCoord(rect, rEvent))
        {
            if (m_onPopTypeSelected)
            {
                m_onPopTypeSelected(*m_popTypes[i]);
                m_bShouldClose = true;
            }
            return;
        }
    }
}

} // namespace ac
