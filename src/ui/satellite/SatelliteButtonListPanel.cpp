#include "ui/satellite/SatelliteButtonListPanel.h"

#include "ui/satellite/SatelliteLabeledButton.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

SatelliteButtonListPanel::SatelliteButtonListPanel(
    WindowLayout_t layout,
    std::string header,
    std::vector<Item_t> items,
    std::optional<std::string> selectedId,
    std::function<void(const std::string&)> onSelect)
    : UIElement(layout)
    , m_header(std::move(header))
    , m_items(std::move(items))
    , m_selectedId(std::move(selectedId))
    , m_onSelect(std::move(onSelect))
{
    RebuildButtons_();
}

void SatelliteButtonListPanel::RebuildButtons_()
{
    m_buttons.clear();

    const auto& style = Style().satelliteView;
    const float padX = style.listPadX;
    const float padY = style.listPadY;
    const float headerH = style.listHeaderHeight;
    const float buttonH = style.listButtonHeight;
    const float gap = style.listButtonGap;
    const float buttonW = m_layout.width - 2.0f * padX;

    float y = m_layout.y + padY + headerH + gap;
    for (const Item_t& rItem : m_items)
    {
        const bool bSelected = m_selectedId && *m_selectedId == rItem.id;
        const std::string id = rItem.id;
        m_buttons.push_back(std::make_unique<SatelliteLabeledButton>(
            WindowLayout_t{m_layout.x + padX, y, buttonW, buttonH},
            rItem.label,
            [this, id]() {
                if (m_onSelect)
                {
                    m_onSelect(id);
                }
            },
            bSelected));
        y += buttonH + gap;
    }
}

void SatelliteButtonListPanel::SetSelected(const std::optional<std::string>& rSelectedId)
{
    if (m_selectedId == rSelectedId)
    {
        return;
    }
    m_selectedId = rSelectedId;
    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        m_buttons[i]->SetSelected(m_selectedId && *m_selectedId == m_items[i].id);
    }
}

void SatelliteButtonListPanel::SetItems(std::vector<Item_t> items,
                                        const std::optional<std::string>& rSelectedId)
{
    m_items = std::move(items);
    m_selectedId = rSelectedId;
    RebuildButtons_();
}

void SatelliteButtonListPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().satelliteView;
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    const float padX = style.listPadX;
    const float padY = style.listPadY;
    rGraphics.DrawText(
        m_header,
        m_layout.x + padX,
        m_layout.y + padY,
        style.headerFontSize,
        style.headerColor);

    if (m_buttons.empty())
    {
        rGraphics.DrawText(
            "None",
            m_layout.x + padX,
            m_layout.y + padY + style.listHeaderHeight + style.listButtonGap,
            style.factionFontSize,
            style.cellColor);
    }

    for (const auto& pButton : m_buttons)
    {
        pButton->Render(rGraphics);
    }
}

void SatelliteButtonListPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const float x = static_cast<float>(rEvent.x);
    const float y = static_cast<float>(rEvent.y);
    for (auto& pButton : m_buttons)
    {
        if (pButton->Contains(x, y))
        {
            pButton->HandleMouseClick(rEvent);
            return;
        }
    }
}

} // namespace ac
