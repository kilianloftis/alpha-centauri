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
#include "game/effects/EffectConfig.h"
#include "ui/style/UiStyle.h"

#include <magic_enum.hpp>

#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

// Derived from the enums, so a new category or rating axis appears here without an edit.
constexpr auto k_Categories = magic_enum::enum_values<SocialCategory_t>();
constexpr auto k_AllRatings = magic_enum::enum_values<SocialRatingId_t>();

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
    const Color_t policyColor = isActive
        ? Style().socialEngineeringDisplay.activePolicyColor
        : Style().socialEngineeringDisplay.inactivePolicyColor;

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
        rGraphics.DrawRect(
            rCell.x, rCell.y, rCell.width, rCell.height,
            Style().socialEngineeringDisplay.activePolicyColor
        );
    }
}

WindowLayout_t GetPolicyGridLayout(const WindowLayout_t& displayLayout)
{
    return ResolveLayout(displayLayout, {
        0.0f,
        0.0f,
        1.0f - Style().socialEngineeringDisplay.scoresPanelWidthRatio,
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
        static_cast<float>(categoryIndex) * Style().socialEngineeringDisplay.categoryRowHeightRatio,
        1.0f,
        Style().socialEngineeringDisplay.categoryRowHeightRatio
    });
    const WindowLayout_t policyRowsBand = ResolveLayout(categoryBand, {
        0.0f,
        Style().socialEngineeringDisplay.categoryNameRowRatio,
        1.0f,
        Style().socialEngineeringDisplay.policyNameRowRatio
            + Style().socialEngineeringDisplay.policyBonusRowRatio
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
        Style().socialEngineeringDisplay.backgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().socialEngineeringDisplay.borderColor
    );

    const WindowLayout_t policyGridLayout = GetPolicyGridLayout(m_layout);
    const WindowLayout_t scoresLayout = ResolveLayout(m_layout, {
        1.0f - Style().socialEngineeringDisplay.scoresPanelWidthRatio,
        0.0f,
        Style().socialEngineeringDisplay.scoresPanelWidthRatio,
        1.0f
    });

    const unsigned int categoryFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.categoryFontSizeRatio);
    const unsigned int policyFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.policyFontSizeRatio);
    const unsigned int bonusFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.bonusFontSizeRatio);
    const unsigned int scoreFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.scoreFontSizeRatio);
    const unsigned int headerFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.headerFontSizeRatio);
    const unsigned int factionNameFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.factionNameFontSizeRatio);
    const unsigned int factionBonusFontSize = static_cast<unsigned int>(
        m_layout.height * Style().socialEngineeringDisplay.factionBonusFontSizeRatio);

    const float horizontalPadding =
        policyGridLayout.width * Style().socialEngineeringDisplay.horizontalPaddingRatio;
    const float verticalPadding =
        policyGridLayout.height * Style().socialEngineeringDisplay.verticalPaddingRatio;

    const std::vector<std::string>& discoveredTechIds = m_pFaction->GetResearch().GetDiscoveredTechs();

    for (size_t categoryIndex = 0; categoryIndex < k_Categories.size(); ++categoryIndex)
    {
        const SocialCategory_t category = k_Categories[categoryIndex];
        const WindowLayout_t categoryBand = ResolveLayout(policyGridLayout, {
            0.0f,
            static_cast<float>(categoryIndex) * Style().socialEngineeringDisplay.categoryRowHeightRatio,
            1.0f,
            Style().socialEngineeringDisplay.categoryRowHeightRatio
        });

        const WindowLayout_t categoryNameRow = ResolveLayout(categoryBand, {
            0.0f,
            0.0f,
            1.0f,
            Style().socialEngineeringDisplay.categoryNameRowRatio
        });

        rGraphics.DrawText(
            CategoryDisplayName(category),
            categoryNameRow.x + horizontalPadding,
            categoryNameRow.y + verticalPadding,
            categoryFontSize,
            Style().socialEngineeringDisplay.categoryColor
        );

        const std::vector<const SocialPolicyConfig_t*> policies = m_pPolicyRegistry->GetByCategory(category);
        const SocialPolicyConfig_t* pActivePolicy = m_pFaction->GetSocialEngineering().GetActivePolicy(category);

        const WindowLayout_t policyRowsBand = ResolveLayout(categoryBand, {
            0.0f,
            Style().socialEngineeringDisplay.categoryNameRowRatio,
            1.0f,
            Style().socialEngineeringDisplay.policyNameRowRatio
                + Style().socialEngineeringDisplay.policyBonusRowRatio
        });
        const float nameRowHeight = policyRowsBand.height * (
            Style().socialEngineeringDisplay.policyNameRowRatio
            / (Style().socialEngineeringDisplay.policyNameRowRatio
                + Style().socialEngineeringDisplay.policyBonusRowRatio));

        for (size_t policyIndex = 0; policyIndex < policies.size(); ++policyIndex)
        {
            const WindowLayout_t policyCell = GetPolicyCellLayout(
                m_layout, categoryIndex, policyIndex, policies.size());

            const SocialPolicyConfig_t* pPolicy = policies[policyIndex];
            if (!pPolicy || !pPolicy->IsAvailable(discoveredTechIds))
            {
                rGraphics.DrawRect(
                    policyCell.x, policyCell.y, policyCell.width, policyCell.height,
                    Style().socialEngineeringDisplay.hiddenPolicySlotColor
                );
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
        1.0f - Style().socialEngineeringDisplay.factionBonusSectionRatio
    });
    const WindowLayout_t factionBonusLayout = ResolveLayout(scoresLayout, {
        0.0f,
        1.0f - Style().socialEngineeringDisplay.factionBonusSectionRatio,
        1.0f,
        Style().socialEngineeringDisplay.factionBonusSectionRatio
    });

    rGraphics.DrawText(
        "Social Scores",
        scoresListLayout.x + horizontalPadding,
        scoresListLayout.y + verticalPadding,
        headerFontSize,
        Style().socialEngineeringDisplay.scoreHeaderColor
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
            Style().socialEngineeringDisplay.scoreValueColor
        );
    }

    const WindowLayout_t factionNameRow = ResolveLayout(factionBonusLayout, {
        0.0f,
        0.0f,
        1.0f,
        Style().socialEngineeringDisplay.factionNameRowRatio
    });
    const WindowLayout_t factionBonusRow = ResolveLayout(factionBonusLayout, {
        0.0f,
        Style().socialEngineeringDisplay.factionNameRowRatio,
        1.0f,
        Style().socialEngineeringDisplay.factionBonusRowRatio
    });

    rGraphics.DrawRect(
        factionBonusLayout.x,
        factionBonusLayout.y,
        factionBonusLayout.width,
        factionBonusLayout.height,
        Style().socialEngineeringDisplay.borderColor
    );

    rGraphics.DrawText(
        GetFactionDisplayName(*m_pFaction),
        factionNameRow.x + horizontalPadding,
        factionNameRow.y + verticalPadding,
        factionNameFontSize,
        Style().socialEngineeringDisplay.scoreHeaderColor
    );

    rGraphics.DrawText(
        FormatFactionBonuses(*m_pFaction, *m_pRatingRegistry),
        factionBonusRow.x + horizontalPadding,
        factionBonusRow.y + verticalPadding,
        factionBonusFontSize,
        Style().socialEngineeringDisplay.factionBonusBodyColor
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
