#include "ui/social-engineering/SocialEngineeringBottomPanel.h"

#include "game/Faction.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

std::string FormatTurnCount(std::optional<int> turns)
{
    if (!turns.has_value())
    {
        return "N/A";
    }

    std::ostringstream oss;
    oss << *turns << (*turns == 1 ? " turn" : " turns");
    return oss.str();
}

} // namespace

SocialEngineeringBottomPanel::SocialEngineeringBottomPanel(
    const Faction& rFaction,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_rFaction(rFaction)
{}

void SocialEngineeringBottomPanel::Render(Graphics& rGraphics)
{

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().socialEngineeringBottomPanel.backgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().socialEngineeringBottomPanel.borderColor
    );

    const unsigned int valueFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringBottomPanel.valueFontSizeRatio);
    const float horizontalPadding =
        m_layout.width * Style().socialEngineeringBottomPanel.horizontalPaddingRatio;
    const float verticalPadding =
        m_layout.height * Style().socialEngineeringBottomPanel.verticalPaddingRatio;

    const WindowLayout_t incomeRow = ResolveLayout(m_layout, {
        0.0f,
        0.0f,
        1.0f,
        Style().socialEngineeringBottomPanel.rowHeightRatio
    });
    const WindowLayout_t breakthroughRow = ResolveLayout(m_layout, {
        0.0f,
        Style().socialEngineeringBottomPanel.rowHeightRatio,
        1.0f,
        Style().socialEngineeringBottomPanel.rowHeightRatio
    });

    std::ostringstream oss;
    oss << "Net Income: " << m_rFaction.GetNetIncomePerTurn() << " / turn";
    rGraphics.DrawText(
        oss.str(),
        incomeRow.x + horizontalPadding,
        incomeRow.y + verticalPadding,
        valueFontSize,
        Style().socialEngineeringBottomPanel.valueColor
    );

    oss.str("");
    // GetTurnsUntilBreakthrough, not GetBreakthroughRate: the label promises turns remaining,
    // and the rate ignores accumulated progress, so the figure only ever read too high.
    oss << "Research Breakthrough: "
        << FormatTurnCount(m_rFaction.GetTurnsUntilBreakthrough());
    rGraphics.DrawText(
        oss.str(),
        breakthroughRow.x + horizontalPadding,
        breakthroughRow.y + verticalPadding,
        valueFontSize,
        Style().socialEngineeringBottomPanel.valueColor
    );
}

} // namespace ac
