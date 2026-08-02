#include "ui/council/CouncilBallotPopup.h"
#include "game/Faction.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

#include <memory>

namespace ac
{

CouncilBallotPopup::CouncilBallotPopup(
    WindowLayout_t layout,
    Mode_t mode,
    std::vector<std::string> labels,
    std::vector<CouncilBallot_t> standardOptions,
    std::vector<Faction*> electionOptions,
    StandardCallback_t onStandard,
    ElectionCallback_t onElection
)
    : UIElement(layout)
    , m_mode(mode)
    , m_labels(std::move(labels))
    , m_standardOptions(std::move(standardOptions))
    , m_electionOptions(std::move(electionOptions))
    , m_onStandard(std::move(onStandard))
    , m_onElection(std::move(onElection))
{
    CacheEntryRects_();
}

std::unique_ptr<CouncilBallotPopup> CouncilBallotPopup::CreateStandard(
    WindowLayout_t layout,
    StandardCallback_t onSelected
)
{
    return std::unique_ptr<CouncilBallotPopup>(new CouncilBallotPopup(
        layout,
        Mode_t::Standard,
        {"Yea", "Nay", "Abstain"},
        {CouncilBallot_t::Yea, CouncilBallot_t::Nay, CouncilBallot_t::Abstain},
        {},
        std::move(onSelected),
        {}));
}

std::unique_ptr<CouncilBallotPopup> CouncilBallotPopup::CreateElection(
    WindowLayout_t layout,
    std::vector<Faction*> candidates,
    ElectionCallback_t onSelected
)
{
    std::vector<std::string> labels;
    std::vector<Faction*> options;
    labels.reserve(candidates.size() + 1);
    options.reserve(candidates.size() + 1);
    for (Faction* pCandidate : candidates)
    {
        if (!pCandidate)
        {
            continue;
        }
        labels.push_back(pCandidate->GetDefinition().identity.name);
        options.push_back(pCandidate);
    }
    labels.push_back("Abstain");
    options.push_back(nullptr);

    return std::unique_ptr<CouncilBallotPopup>(new CouncilBallotPopup(
        layout,
        Mode_t::Election,
        std::move(labels),
        {},
        std::move(options),
        {},
        std::move(onSelected)));
}

void CouncilBallotPopup::CacheEntryRects_()
{
    const auto& style = Style().productionSelectorPopup;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    float offsetY = lineHeight * style.headerLineOffset;
    for (size_t i = 0; i < m_labels.size(); ++i)
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

void CouncilBallotPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().productionSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText(
        "Cast Vote",
        m_layout.x + padding,
        m_layout.y + padding,
        headerFontSize,
        style.headerColor);

    for (size_t i = 0; i < m_labels.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_labels[i], rect.x + padding, rect.y, entryFontSize, style.entryColor);
    }
}

bool CouncilBallotPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CouncilBallotPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (!ContainsMouseCoord(m_layout, rEvent))
    {
        m_bShouldClose = true;
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        if (!ContainsMouseCoord(m_entryRects[i], rEvent))
        {
            continue;
        }
        if (m_mode == Mode_t::Standard && m_onStandard)
        {
            m_onStandard(m_standardOptions[i]);
        }
        else if (m_mode == Mode_t::Election && m_onElection)
        {
            m_onElection(m_electionOptions[i]);
        }
        m_bShouldClose = true;
        return;
    }
}

} // namespace ac
