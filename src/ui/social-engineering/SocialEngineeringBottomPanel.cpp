#include "ui/social-engineering/SocialEngineeringBottomPanel.h"

#include "game/Faction.h"
#include "graphics/Graphics.h"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor          {20, 20, 30, 255};
constexpr Color_t k_BorderColor              {80, 80, 120, 255};
constexpr Color_t k_ValueColor               {255, 255, 255, 255};

constexpr float k_RowHeightRatio           = 0.5f;
constexpr float k_HorizontalPaddingRatio   = 0.02f;
constexpr float k_VerticalPaddingRatio     = 0.08f;
constexpr float k_ValueFontSizeRatio       = 0.07f;

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
    const Faction* pFaction,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_pFaction(pFaction)
{}

void SocialEngineeringBottomPanel::Render(Graphics& rGraphics)
{
    if (!m_pFaction)
    {
        throw std::runtime_error("SocialEngineeringBottomPanel: No faction set");
    }

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        k_BackgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        k_BorderColor
    );

    const unsigned int valueFontSize = static_cast<unsigned int>(m_layout.height * k_ValueFontSizeRatio);
    const float horizontalPadding = m_layout.width * k_HorizontalPaddingRatio;
    const float verticalPadding = m_layout.height * k_VerticalPaddingRatio;

    const WindowLayout_t incomeRow = ResolveLayout(m_layout, {
        0.0f,
        0.0f,
        1.0f,
        k_RowHeightRatio
    });
    const WindowLayout_t breakthroughRow = ResolveLayout(m_layout, {
        0.0f,
        k_RowHeightRatio,
        1.0f,
        k_RowHeightRatio
    });

    std::ostringstream oss;
    oss << "Net Income: " << m_pFaction->GetNetIncomePerTurn() << " / turn";
    rGraphics.DrawText(
        oss.str(),
        incomeRow.x + horizontalPadding,
        incomeRow.y + verticalPadding,
        valueFontSize,
        k_ValueColor
    );

    oss.str("");
    oss << "Research Breakthrough: " << FormatTurnCount(m_pFaction->GetBreakthroughRate());
    rGraphics.DrawText(
        oss.str(),
        breakthroughRow.x + horizontalPadding,
        breakthroughRow.y + verticalPadding,
        valueFontSize,
        k_ValueColor
    );
}

} // namespace ac
