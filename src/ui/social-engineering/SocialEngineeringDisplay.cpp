#include "ui/social-engineering/SocialEngineeringDisplay.h"

#include "game/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "graphics/Graphics.h"
#include "game/effects/BonusEffect.h"

#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor           {20, 20, 30, 255};
constexpr Color_t k_BorderColor               {80, 80, 120, 255};
constexpr Color_t k_ActivePolicyColor         {255, 220, 80, 255};
constexpr Color_t k_InactivePolicyColor       {200, 200, 200, 255};
constexpr Color_t k_CategoryColor             {160, 180, 255, 255};
constexpr Color_t k_ScoreHeaderColor          {160, 180, 255, 255};

constexpr float k_ScoresPanelWidthRatio     = 0.2f;
constexpr float k_CategoryRowHeightRatio    = 0.25f;
constexpr float k_CategoryNameRowRatio      = 0.28f;
constexpr float k_PolicyNameRowRatio        = 0.36f;
constexpr float k_PolicyBonusRowRatio       = 0.36f;
constexpr float k_HorizontalPaddingRatio    = 0.01f;
constexpr float k_VerticalPaddingRatio      = 0.02f;
constexpr float k_HeaderFontSizeRatio       = 0.045f;
constexpr float k_CategoryFontSizeRatio     = 0.04f;
constexpr float k_PolicyFontSizeRatio       = 0.032f;
constexpr float k_BonusFontSizeRatio        = 0.026f;
constexpr float k_ScoreFontSizeRatio        = 0.028f;
constexpr float k_FactionBonusSectionRatio  = 0.18f;
constexpr float k_FactionNameRowRatio       = 0.45f;
constexpr float k_FactionBonusRowRatio      = 0.55f;
constexpr float k_FactionNameFontSizeRatio  = 0.034f;
constexpr float k_FactionBonusFontSizeRatio = 0.024f;

constexpr Color_t k_HiddenPolicySlotColor       {50, 50, 65, 255};

constexpr std::array<SocialCategory_t, 4> k_Categories = {
    SocialCategory_t::Politics,
    SocialCategory_t::Economics,
    SocialCategory_t::Values,
    SocialCategory_t::FutureSociety
};

constexpr std::array<SocialRatingId_t, 10> k_AllRatings = {
    SocialRatingId_t::Economy,
    SocialRatingId_t::Efficiency,
    SocialRatingId_t::Support,
    SocialRatingId_t::Police,
    SocialRatingId_t::Morale,
    SocialRatingId_t::Growth,
    SocialRatingId_t::Planet,
    SocialRatingId_t::Research,
    SocialRatingId_t::Industry,
    SocialRatingId_t::Probe
};

std::string CapitalizeFirst(std::string text)
{
    if (!text.empty())
    {
        text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    }
    return text;
}

std::string CategoryDisplayName(SocialCategory_t category)
{
    switch (category)
    {
        case SocialCategory_t::Politics:       return "Politics";
        case SocialCategory_t::Economics:      return "Economics";
        case SocialCategory_t::Values:         return "Values";
        // Display label differs from enumerator name (space inserted).
        case SocialCategory_t::FutureSociety:  return "Future Society";
    }
    throw std::runtime_error("Unknown SocialCategory_t");
}

std::string RatingDisplayName(SocialRatingId_t rating)
{
    return CapitalizeFirst(SocialRatingIdToString(rating));
}

std::string FormatPolicyBonuses(const SocialPolicyConfig_t& rPolicy)
{
    std::ostringstream oss;
    bool first = true;
    for (const EffectConfig_t& rEffect : rPolicy.effects)
    {
        if (const auto* pRatingMod = std::get_if<SocialRatingModifierEffect_t>(&rEffect.effect))
        {
            if (!first)
            {
                oss << ", ";
            }
            first = false;
            oss << (pRatingMod->amount >= 0 ? "+" : "") << pRatingMod->amount << " "
                << RatingDisplayName(pRatingMod->rating);
        }
    }

    return first ? "None" : oss.str();
}

std::string GetFactionDisplayName(const Faction& rFaction)
{
    const FactionConfig_t& rDefinition = rFaction.GetDefinition();
    if (!rDefinition.identity.name.empty())
    {
        return rDefinition.identity.name;
    }
    if (!rDefinition.id.empty())
    {
        return CapitalizeFirst(rDefinition.id);
    }
    return "Unknown";
}

std::string FormatFactionBonuses(
    const Faction& rFaction,
    const SocialRatingRegistry& rRegistry
)
{
    std::ostringstream oss;
    bool first = true;

    for (const SocialRatingId_t rating : k_AllRatings)
    {
        const int level = rFaction.GetSocialEngineering().GetSocialRating(rating);
        if (level == 0)
        {
            continue;
        }

        const SocialRatingConfig_t* pRatingConfig = rRegistry.Find(SocialRatingIdToString(rating));
        if (!pRatingConfig)
        {
            continue;
        }

        const std::vector<EffectConfig_t>* pLevelEffects =
            FindSocialRatingLevelEffects(*pRatingConfig, level);
        if (!pLevelEffects)
        {
            continue;
        }
    }

    return first ? "None" : oss.str();
}

void RenderPolicyCell(
    Graphics& rGraphics,
    const WindowLayout_t& rCell,
    const SocialPolicyConfig_t& rPolicy,
    bool isActive,
    unsigned int policyFontSize,
    unsigned int bonusFontSize,
    float horizontalPadding,
    float nameRowHeight
)
{
    const Color_t policyColor = isActive ? k_ActivePolicyColor : k_InactivePolicyColor;

    rGraphics.DrawText(
        rPolicy.name,
        rCell.x + horizontalPadding,
        rCell.y,
        policyFontSize,
        policyColor
    );

    rGraphics.DrawText(
        FormatPolicyBonuses(rPolicy),
        rCell.x + horizontalPadding,
        rCell.y + nameRowHeight,
        bonusFontSize,
        policyColor
    );

    if (isActive)
    {
        rGraphics.DrawRect(rCell.x, rCell.y, rCell.width, rCell.height, k_ActivePolicyColor);
    }
}

WindowLayout_t GetPolicyGridLayout(const WindowLayout_t& displayLayout)
{
    return ResolveLayout(displayLayout, {
        0.0f,
        0.0f,
        1.0f - k_ScoresPanelWidthRatio,
        1.0f
    });
}

WindowLayout_t GetPolicyCellLayout(
    const WindowLayout_t& displayLayout,
    size_t categoryIndex,
    size_t policyIndex,
    size_t policyCount
)
{
    if (policyCount == 0)
    {
        throw std::runtime_error("GetPolicyCellLayout: policyCount must be > 0");
    }

    const WindowLayout_t policyGridLayout = GetPolicyGridLayout(displayLayout);
    const WindowLayout_t categoryBand = ResolveLayout(policyGridLayout, {
        0.0f,
        static_cast<float>(categoryIndex) * k_CategoryRowHeightRatio,
        1.0f,
        k_CategoryRowHeightRatio
    });
    const WindowLayout_t policyRowsBand = ResolveLayout(categoryBand, {
        0.0f,
        k_CategoryNameRowRatio,
        1.0f,
        k_PolicyNameRowRatio + k_PolicyBonusRowRatio
    });

    return ResolveLayout(policyRowsBand, {
        static_cast<float>(policyIndex) / static_cast<float>(policyCount),
        0.0f,
        1.0f / static_cast<float>(policyCount),
        1.0f
    });
}

const SocialPolicyConfig_t* FindAvailablePolicyAt(
    const SocialPolicyRegistry& rRegistry,
    const std::vector<std::string>& rDiscoveredTechIds,
    SocialCategory_t category,
    size_t policyIndex
)
{
    const std::vector<const SocialPolicyConfig_t*> policies = rRegistry.GetByCategory(category);
    if (policyIndex >= policies.size())
    {
        return nullptr;
    }

    const SocialPolicyConfig_t* pPolicy = policies[policyIndex];
    if (!pPolicy || !pPolicy->IsAvailable(rDiscoveredTechIds))
    {
        return nullptr;
    }

    return pPolicy;
}

} // namespace

SocialEngineeringDisplay::SocialEngineeringDisplay(
    Faction* pFaction,
    const SocialPolicyRegistry* pPolicyRegistry,
    const SocialRatingRegistry* pRatingRegistry,
    WindowLayout_t layout
)
    : UIElement(layout)
    , m_pFaction(pFaction)
    , m_pPolicyRegistry(pPolicyRegistry)
    , m_pRatingRegistry(pRatingRegistry)
{}

void SocialEngineeringDisplay::Render(Graphics& rGraphics)
{
    if (!m_pFaction)
    {
        throw std::runtime_error("SocialEngineeringDisplay: No faction set");
    }
    if (!m_pPolicyRegistry)
    {
        throw std::runtime_error("SocialEngineeringDisplay: No policy registry set");
    }
    if (!m_pRatingRegistry)
    {
        throw std::runtime_error("SocialEngineeringDisplay: No rating registry set");
    }

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        k_BackgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        k_BorderColor
    );

    const WindowLayout_t policyGridLayout = GetPolicyGridLayout(m_layout);
    const WindowLayout_t scoresLayout = ResolveLayout(m_layout, {
        1.0f - k_ScoresPanelWidthRatio,
        0.0f,
        k_ScoresPanelWidthRatio,
        1.0f
    });

    const unsigned int categoryFontSize = static_cast<unsigned int>(m_layout.height * k_CategoryFontSizeRatio);
    const unsigned int policyFontSize   = static_cast<unsigned int>(m_layout.height * k_PolicyFontSizeRatio);
    const unsigned int bonusFontSize    = static_cast<unsigned int>(m_layout.height * k_BonusFontSizeRatio);
    const unsigned int scoreFontSize        = static_cast<unsigned int>(m_layout.height * k_ScoreFontSizeRatio);
    const unsigned int headerFontSize       = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int factionNameFontSize  = static_cast<unsigned int>(m_layout.height * k_FactionNameFontSizeRatio);
    const unsigned int factionBonusFontSize = static_cast<unsigned int>(m_layout.height * k_FactionBonusFontSizeRatio);

    const float horizontalPadding = policyGridLayout.width * k_HorizontalPaddingRatio;
    const float verticalPadding   = policyGridLayout.height * k_VerticalPaddingRatio;

    const std::vector<std::string>& discoveredTechIds = m_pFaction->GetResearch().GetDiscoveredTechs();

    for (size_t categoryIndex = 0; categoryIndex < k_Categories.size(); ++categoryIndex)
    {
        const SocialCategory_t category = k_Categories[categoryIndex];
        const WindowLayout_t categoryBand = ResolveLayout(policyGridLayout, {
            0.0f,
            static_cast<float>(categoryIndex) * k_CategoryRowHeightRatio,
            1.0f,
            k_CategoryRowHeightRatio
        });

        const WindowLayout_t categoryNameRow = ResolveLayout(categoryBand, {
            0.0f,
            0.0f,
            1.0f,
            k_CategoryNameRowRatio
        });

        rGraphics.DrawText(
            CategoryDisplayName(category),
            categoryNameRow.x + horizontalPadding,
            categoryNameRow.y + verticalPadding,
            categoryFontSize,
            k_CategoryColor
        );

        const std::vector<const SocialPolicyConfig_t*> policies = m_pPolicyRegistry->GetByCategory(category);
        const SocialPolicyConfig_t* pActivePolicy = m_pFaction->GetSocialEngineering().GetActivePolicy(category);

        const WindowLayout_t policyRowsBand = ResolveLayout(categoryBand, {
            0.0f,
            k_CategoryNameRowRatio,
            1.0f,
            k_PolicyNameRowRatio + k_PolicyBonusRowRatio
        });
        const float nameRowHeight = policyRowsBand.height * (k_PolicyNameRowRatio / (k_PolicyNameRowRatio + k_PolicyBonusRowRatio));

        for (size_t policyIndex = 0; policyIndex < policies.size(); ++policyIndex)
        {
            const WindowLayout_t policyCell = GetPolicyCellLayout(
                m_layout, categoryIndex, policyIndex, policies.size());

            const SocialPolicyConfig_t* pPolicy = policies[policyIndex];
            if (!pPolicy || !pPolicy->IsAvailable(discoveredTechIds))
            {
                rGraphics.DrawRect(policyCell.x, policyCell.y, policyCell.width, policyCell.height, k_HiddenPolicySlotColor);
                continue;
            }

            const bool isActive = pActivePolicy && pActivePolicy->id == pPolicy->id;
            RenderPolicyCell(
                rGraphics,
                policyCell,
                *pPolicy,
                isActive,
                policyFontSize,
                bonusFontSize,
                horizontalPadding,
                nameRowHeight
            );
        }
    }

    const WindowLayout_t scoresListLayout = ResolveLayout(scoresLayout, {
        0.0f,
        0.0f,
        1.0f,
        1.0f - k_FactionBonusSectionRatio
    });
    const WindowLayout_t factionBonusLayout = ResolveLayout(scoresLayout, {
        0.0f,
        1.0f - k_FactionBonusSectionRatio,
        1.0f,
        k_FactionBonusSectionRatio
    });

    rGraphics.DrawText(
        "Social Scores",
        scoresListLayout.x + horizontalPadding,
        scoresListLayout.y + verticalPadding,
        headerFontSize,
        k_ScoreHeaderColor
    );

    const float scoreLineHeight = scoresListLayout.height / static_cast<float>(k_AllRatings.size() + 1);
    for (size_t ratingIndex = 0; ratingIndex < k_AllRatings.size(); ++ratingIndex)
    {
        const SocialRatingId_t rating = k_AllRatings[ratingIndex];
        const int score = m_pFaction->GetSocialEngineering().GetSocialRating(rating);

        std::ostringstream oss;
        oss << RatingDisplayName(rating) << ": " << score;
        rGraphics.DrawText(
            oss.str(),
            scoresListLayout.x + horizontalPadding,
            scoresListLayout.y + scoreLineHeight * static_cast<float>(ratingIndex + 1),
            scoreFontSize,
            Color_t::White()
        );
    }

    const WindowLayout_t factionNameRow = ResolveLayout(factionBonusLayout, {
        0.0f,
        0.0f,
        1.0f,
        k_FactionNameRowRatio
    });
    const WindowLayout_t factionBonusRow = ResolveLayout(factionBonusLayout, {
        0.0f,
        k_FactionNameRowRatio,
        1.0f,
        k_FactionBonusRowRatio
    });

    rGraphics.DrawRect(
        factionBonusLayout.x,
        factionBonusLayout.y,
        factionBonusLayout.width,
        factionBonusLayout.height,
        k_BorderColor
    );

    rGraphics.DrawText(
        GetFactionDisplayName(*m_pFaction),
        factionNameRow.x + horizontalPadding,
        factionNameRow.y + verticalPadding,
        factionNameFontSize,
        k_ScoreHeaderColor
    );

    rGraphics.DrawText(
        FormatFactionBonuses(*m_pFaction, *m_pRatingRegistry),
        factionBonusRow.x + horizontalPadding,
        factionBonusRow.y + verticalPadding,
        factionBonusFontSize,
        Color_t::White()
    );
}

void SocialEngineeringDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left || !m_pFaction || !m_pPolicyRegistry)
    {
        return;
    }

    const std::vector<std::string>& discoveredTechIds = m_pFaction->GetResearch().GetDiscoveredTechs();

    const float clickX = static_cast<float>(rEvent.x);
    const float clickY = static_cast<float>(rEvent.y);

    for (size_t categoryIndex = 0; categoryIndex < k_Categories.size(); ++categoryIndex)
    {
        const SocialCategory_t category = k_Categories[categoryIndex];
        const std::vector<const SocialPolicyConfig_t*> policies =
            m_pPolicyRegistry->GetByCategory(category);
        if (policies.empty())
        {
            continue;
        }

        for (size_t policyIndex = 0; policyIndex < policies.size(); ++policyIndex)
        {
            const WindowLayout_t policyCell = GetPolicyCellLayout(
                m_layout, categoryIndex, policyIndex, policies.size());
            if (!ContainsMouseCoord(policyCell, clickX, clickY))
            {
                continue;
            }

            const SocialPolicyConfig_t* pPolicy = FindAvailablePolicyAt(
                *m_pPolicyRegistry,
                discoveredTechIds,
                category,
                policyIndex
            );
            if (!pPolicy)
            {
                return;
            }

            m_pFaction->GetSocialEngineering().SetActivePolicy(*pPolicy);
            return;
        }
    }
}

} // namespace ac
