#include "ui/base/CommerceDisplay.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/CommerceCalculator.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseManager.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ac
{
namespace
{

std::string Uppercase_(std::string text)
{
    for (char& rCh : text)
    {
        rCh = static_cast<char>(std::toupper(static_cast<unsigned char>(rCh)));
    }
    return text;
}

} // namespace

CommerceDisplay::CommerceDisplay(BaseManager& rBase, WindowLayout_t layout)
    : UIElement(layout)
    , m_rBase(rBase)
{}

void CommerceDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().commerceDisplay;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);

    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    const float leftPadding = m_layout.width * style.leftPaddingRatio;

    rGraphics.DrawText(
        "Commerce", m_layout.x + leftPadding, m_layout.y, headerFontSize, style.textColor);

    GameState* pState = m_rBase.GetFaction().GetGameState();
    if (pState == nullptr)
    {
        return;
    }

    float lineIndex = 1.0f;
    for (const CommercePartnerLine_t& rLine :
         CommerceCalculator{}.ComputeForBase(m_rBase, *pState))
    {
        if (rLine.pPartner == nullptr)
        {
            throw std::runtime_error("CommerceDisplay: partner line has null faction");
        }

        const std::string shorthand =
            Uppercase_(rLine.pPartner->GetDefinition().identity.name);

        std::ostringstream factionLine;
        factionLine << shorthand << ": " << rLine.ourEnergy;
        rGraphics.DrawText(
            factionLine.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * lineIndex,
            entryFontSize, style.textColor);
        lineIndex += 1.0f;

        std::ostringstream treatyLine;
        treatyLine << ToString(rLine.status) << " (They get " << rLine.theirEnergy << ")";
        rGraphics.DrawText(
            treatyLine.str(), m_layout.x + leftPadding, m_layout.y + lineHeight * lineIndex,
            entryFontSize, style.textColor);
        lineIndex += 1.0f;
    }
}

} // namespace ac
