#include "ui/social-engineering/SocialEngineeringDisplay.h"

#include "game/social-engineering/SocialEngineeringMutations.h"
#include "game/social-engineering/SocialEngineeringRules.h"

#include "game/economy/EconomyManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyDraft.h"
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
#include <map>
#include <optional>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

// Derived from the enums, so a new category or rating axis appears here without an edit.
constexpr auto k_Categories = magic_enum::enum_values<SocialCategory_t>();
constexpr auto k_AllRatings = magic_enum::enum_values<SocialRatingId_t>();

// Trims a trailing ".0" so whole numbers read as "+2" rather than "+2.000000".
std::string FormatAmount(double amount)
{
    std::ostringstream oss;
    if (amount == static_cast<long long>(amount))
    {
        oss << static_cast<long long>(amount);
    }
    else
    {
        oss << amount;
    }
    return oss.str();
}

std::string CapitalizeFirst(std::string text)
{
    if (!text.empty())
    {
        text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    }
    return text;
}

// "GrowthRate" -> "Growth Rate", for enumerator names that reach the player through effect
// payloads (StatId_t, RuleFlagId_t); no display-name table exists for them.
std::string HumanizeEnumName(std::string_view name)
{
    std::string text;
    for (size_t i = 0; i < name.size(); ++i)
    {
        if (i > 0 && std::isupper(static_cast<unsigned char>(name[i]))
            && !std::isupper(static_cast<unsigned char>(name[i - 1])))
        {
            text += ' ';
        }
        text += name[i];
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

// One social-rating level effect as a player-facing phrase. Effect kinds with no meaningful
// short form return empty and are skipped.
std::string FormatLevelEffect(const EffectConfig_t& rEffect)
{
    if (const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect))
    {
        // Internal energy-pipeline parameters, not player-facing bonus lines.
        if (pModifier->stat == StatId_t::InefficiencyDenominator
            || pModifier->stat == StatId_t::AllocationImbalanceFactor)
        {
            return {};
        }
        std::ostringstream oss;
        const double amount = pModifier->amount;
        oss << (amount >= 0.0 ? "+" : "") << FormatAmount(amount);
        if (pModifier->op == ModifierOp_t::AddPercent)
        {
            oss << "%";
        }
        oss << " " << HumanizeEnumName(magic_enum::enum_name(pModifier->stat));
        return oss.str();
    }
    if (const auto* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.effect))
    {
        return HumanizeEnumName(magic_enum::enum_name(pFlag->flag));
    }
    return {};
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

        // A non-zero rating whose axis has no table is a config defect, not an empty bonus
        // line: ValidateEffectReferences rejects the modifier that produced it at load.
        const SocialRatingConfig_t& rRatingConfig =
            rRegistry.Get(SocialRatingIdToString(rating));

        const std::vector<EffectConfig_t>* pLevelEffects =
            FindSocialRatingLevelEffects(rRatingConfig, level);
        if (!pLevelEffects)
        {
            continue;
        }

        for (const EffectConfig_t& rEffect : *pLevelEffects)
        {
            const std::string text = FormatLevelEffect(rEffect);
            if (text.empty())
            {
                continue;
            }
            if (!first)
            {
                oss << ", ";
            }
            oss << text;
            first = false;
        }
    }

    return first ? "None" : oss.str();
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
    const Faction& rFaction,
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
    if (!pPolicy || !PolicyAvailable(rFaction, *pPolicy))
    {
        return nullptr;
    }

    return pPolicy;
}

std::map<SocialRatingId_t, int> DraftRatingTotals_(const SocialPolicyDraft_t& rDraft)
{
    std::vector<ActiveEffect_t> effects;
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        const SocialPolicyConfig_t& rPolicy = rDraft.Get(category);
        AppendActiveEffects(rPolicy.effects, nullptr, rPolicy.id, effects);
    }
    return AccumulateSocialRatings(effects, std::nullopt);
}

} // namespace

SocialEngineeringDisplay::SocialEngineeringDisplay(
    Faction& rFaction,
    const SocialPolicyRegistry& rPolicyRegistry,
    const SocialRatingRegistry& rRatingRegistry,
    WindowLayout_t layout,
    ConfirmFn confirm,
    NoticeFn notice,
    CommittedFn onCommitted
)
    : UIElement(layout)
    , m_rFaction(rFaction)
    , m_rPolicyRegistry(rPolicyRegistry)
    , m_rRatingRegistry(rRatingRegistry)
    , m_confirm(std::move(confirm))
    , m_notice(std::move(notice))
    , m_onCommitted(std::move(onCommitted))
    , m_draft(PendingSocialDraft(rFaction))
{
}

void SocialEngineeringDisplay::Render(Graphics& rGraphics)
{
    const auto& s = Style().socialEngineeringDisplay;
    DrawFrame_(rGraphics);
    const WindowLayout_t policyGridLayout = GetPolicyGridLayout(m_layout);
    const Padding_t padding{policyGridLayout.width * s.horizontalPaddingRatio,
                            policyGridLayout.height * s.verticalPaddingRatio};
    for (size_t categoryIndex = 0; categoryIndex < k_Categories.size(); ++categoryIndex)
    {
        DrawCategory_(rGraphics, categoryIndex, policyGridLayout, padding);
    }

    const WindowLayout_t scoresLayout =
        ResolveLayout(m_layout, {1.0f - s.scoresPanelWidthRatio, 0.0f, s.scoresPanelWidthRatio,
                                 1.0f});
    const float bonusRatio = s.factionBonusSectionRatio;
    DrawScores_(rGraphics, ResolveLayout(scoresLayout, {0.0f, 0.0f, 1.0f, 1.0f - bonusRatio}),
                padding);
    DrawFactionBonus_(rGraphics,
                      ResolveLayout(scoresLayout, {0.0f, 1.0f - bonusRatio, 1.0f, bonusRatio}),
                      padding);
}

unsigned int SocialEngineeringDisplay::FontSize_(float heightRatio) const
{
    return static_cast<unsigned int>(m_layout.height * heightRatio);
}

void SocialEngineeringDisplay::DrawFrame_(Graphics& rGraphics) const
{
    const auto& s = Style().socialEngineeringDisplay;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        s.borderColor, Style().outline.width);
}

// The category's name, then a cell per policy: hidden slots for ones the faction cannot pick.
void SocialEngineeringDisplay::DrawCategory_(Graphics& rGraphics, size_t categoryIndex,
                                             const WindowLayout_t& rPolicyGrid,
                                             const Padding_t& rPadding) const
{
    const auto& s = Style().socialEngineeringDisplay;
    const SocialCategory_t category = k_Categories[categoryIndex];
    const WindowLayout_t categoryBand = ResolveLayout(rPolicyGrid, {
        0.0f,
        static_cast<float>(categoryIndex) * s.categoryRowHeightRatio,
        1.0f,
        s.categoryRowHeightRatio
    });
    const WindowLayout_t categoryNameRow =
        ResolveLayout(categoryBand, {0.0f, 0.0f, 1.0f, s.categoryNameRowRatio});
    rGraphics.DrawText(
        CategoryDisplayName(category),
        categoryNameRow.x + rPadding.horizontal,
        categoryNameRow.y + rPadding.vertical,
        FontSize_(s.categoryFontSizeRatio),
        s.categoryColor
    );

    const std::vector<const SocialPolicyConfig_t*> policies =
        m_rPolicyRegistry.GetByCategory(category);
    const SocialPolicyConfig_t& rDraftPolicy = m_draft.Get(category);
    const WindowLayout_t policyRowsBand = ResolveLayout(categoryBand, {
        0.0f,
        s.categoryNameRowRatio,
        1.0f,
        s.policyNameRowRatio + s.policyBonusRowRatio
    });
    const float nameRowHeight = policyRowsBand.height * (
        s.policyNameRowRatio / (s.policyNameRowRatio + s.policyBonusRowRatio));

    for (size_t policyIndex = 0; policyIndex < policies.size(); ++policyIndex)
    {
        const WindowLayout_t policyCell = GetPolicyCellLayout(
            m_layout, categoryIndex, policyIndex, policies.size());
        const SocialPolicyConfig_t* pPolicy = policies[policyIndex];
        if (!pPolicy || !PolicyAvailable(m_rFaction, *pPolicy))
        {
            rGraphics.DrawRect(policyCell.x, policyCell.y, policyCell.width, policyCell.height,
                               s.hiddenPolicySlotColor, Style().outline.width);
            continue;
        }
        DrawPolicyCell_(rGraphics, policyCell, *pPolicy, rDraftPolicy, rPadding.horizontal,
                        nameRowHeight);
    }
}

void SocialEngineeringDisplay::DrawPolicyCell_(Graphics& rGraphics, const WindowLayout_t& rCell,
                                               const SocialPolicyConfig_t& rPolicy,
                                               const SocialPolicyConfig_t& rDraftPolicy,
                                               float horizontalPadding, float nameRowHeight) const
{
    const auto& s = Style().socialEngineeringDisplay;
    const bool bIsDraft = rDraftPolicy.id == rPolicy.id;
    const Color_t policyColor = bIsDraft ? s.activePolicyColor : s.inactivePolicyColor;

    rGraphics.DrawText(rPolicy.name, rCell.x + horizontalPadding, rCell.y,
                       FontSize_(s.policyFontSizeRatio), policyColor);
    rGraphics.DrawText(FormatPolicyBonuses(rPolicy), rCell.x + horizontalPadding,
                       rCell.y + nameRowHeight, FontSize_(s.bonusFontSizeRatio), policyColor);
    if (bIsDraft)
    {
        rGraphics.DrawRect(rCell.x, rCell.y, rCell.width, rCell.height, s.activePolicyColor,
                           Style().outline.width);
    }
}

WindowLayout_t SocialEngineeringDisplay::OkButtonLayout_(
    const WindowLayout_t& rScoresList) const
{
    const auto& s = Style().socialEngineeringDisplay;
    return ResolveLayout(rScoresList, {0.0f, 1.0f - s.scoreFontSizeRatio * 2.0f, 1.0f,
                                       s.scoreFontSizeRatio * 2.0f});
}

void SocialEngineeringDisplay::DrawScores_(Graphics& rGraphics,
                                           const WindowLayout_t& rScoresList,
                                           const Padding_t& rPadding) const
{
    const auto& s = Style().socialEngineeringDisplay;
    rGraphics.DrawText(
        "Social Scores",
        rScoresList.x + rPadding.horizontal,
        rScoresList.y + rPadding.vertical,
        FontSize_(s.headerFontSizeRatio),
        s.scoreHeaderColor
    );
    const SocialEngineeringManager& rSe = m_rFaction.GetSocialEngineering();
    const int upheavalAbsolute = QuoteUpheavalAbsolute(m_rFaction, m_draft);
    const bool bShowUpheaval = upheavalAbsolute != 0;
    const std::map<SocialRatingId_t, int> draftRatings = DraftRatingTotals_(m_draft);
    // Header + ratings + optional upheaval + OK.
    const float lineCount =
        static_cast<float>(k_AllRatings.size() + 2 + (bShowUpheaval ? 1 : 0));
    const float scoreLineHeight = rScoresList.height / lineCount;
    for (size_t ratingIndex = 0; ratingIndex < k_AllRatings.size(); ++ratingIndex)
    {
        const SocialRatingId_t rating = k_AllRatings[ratingIndex];
        const auto it = draftRatings.find(rating);
        const int score = it == draftRatings.end() ? 0 : it->second;
        std::ostringstream oss;
        oss << RatingDisplayName(rating) << ": " << score;
        rGraphics.DrawText(
            oss.str(),
            rScoresList.x + rPadding.horizontal,
            rScoresList.y + scoreLineHeight * static_cast<float>(ratingIndex + 1),
            FontSize_(s.scoreFontSizeRatio),
            s.scoreValueColor
        );
    }
    float nextLine = static_cast<float>(k_AllRatings.size() + 1);
    if (bShowUpheaval)
    {
        const int paid = rSe.GetUpheavalPaid();
        std::ostringstream oss;
        if (upheavalAbsolute >= paid)
        {
            oss << "Upheaval Cost: " << upheavalAbsolute;
        }
        else
        {
            oss << "Upheaval Refund: " << (paid - upheavalAbsolute);
        }
        rGraphics.DrawText(
            oss.str(),
            rScoresList.x + rPadding.horizontal,
            rScoresList.y + scoreLineHeight * nextLine,
            FontSize_(s.scoreFontSizeRatio),
            s.scoreValueColor
        );
    }
    const WindowLayout_t okButton = OkButtonLayout_(rScoresList);
    rGraphics.DrawText(
        "OK",
        okButton.x + rPadding.horizontal,
        okButton.y + rPadding.vertical,
        FontSize_(s.scoreFontSizeRatio),
        s.activePolicyColor
    );
}

void SocialEngineeringDisplay::DrawFactionBonus_(Graphics& rGraphics,
                                                 const WindowLayout_t& rBonusSection,
                                                 const Padding_t& rPadding) const
{
    const auto& s = Style().socialEngineeringDisplay;
    const WindowLayout_t factionNameRow =
        ResolveLayout(rBonusSection, {0.0f, 0.0f, 1.0f, s.factionNameRowRatio});
    const WindowLayout_t factionBonusRow = ResolveLayout(rBonusSection, {
        0.0f,
        s.factionNameRowRatio,
        1.0f,
        s.factionBonusRowRatio
    });
    rGraphics.DrawRect(rBonusSection.x, rBonusSection.y, rBonusSection.width,
                       rBonusSection.height, s.borderColor, Style().outline.width);
    rGraphics.DrawText(
        GetFactionDisplayName(m_rFaction),
        factionNameRow.x + rPadding.horizontal,
        factionNameRow.y + rPadding.vertical,
        FontSize_(s.factionNameFontSizeRatio),
        s.scoreHeaderColor
    );
    rGraphics.DrawText(
        FormatFactionBonuses(m_rFaction, m_rRatingRegistry),
        factionBonusRow.x + rPadding.horizontal,
        factionBonusRow.y + rPadding.vertical,
        FontSize_(s.factionBonusFontSizeRatio),
        s.factionBonusBodyColor
    );
}

void SocialEngineeringDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    const float clickX = static_cast<float>(rEvent.x);
    const float clickY = static_cast<float>(rEvent.y);

    const auto& s = Style().socialEngineeringDisplay;
    const WindowLayout_t scoresLayout =
        ResolveLayout(m_layout, {1.0f - s.scoresPanelWidthRatio, 0.0f, s.scoresPanelWidthRatio,
                                 1.0f});
    const float bonusRatio = s.factionBonusSectionRatio;
    const WindowLayout_t scoresList =
        ResolveLayout(scoresLayout, {0.0f, 0.0f, 1.0f, 1.0f - bonusRatio});
    if (ContainsMouseCoord(OkButtonLayout_(scoresList), clickX, clickY))
    {
        TryCommitDraft();
        return;
    }

    for (size_t categoryIndex = 0; categoryIndex < k_Categories.size(); ++categoryIndex)
    {
        const SocialCategory_t category = k_Categories[categoryIndex];
        const std::vector<const SocialPolicyConfig_t*> policies =
            m_rPolicyRegistry.GetByCategory(category);
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
                m_rPolicyRegistry,
                m_rFaction,
                category,
                policyIndex
            );
            if (!pPolicy)
            {
                return;
            }

            SelectDraftPolicy_(*pPolicy);
            return;
        }
    }
}

void SocialEngineeringDisplay::SelectDraftPolicy_(const SocialPolicyConfig_t& rPolicy)
{
    m_draft.Set(m_rPolicyRegistry.Get(rPolicy.id));
}

void SocialEngineeringDisplay::TryCommitDraft()
{
    const int delta = QuoteUpheavalDelta(m_rFaction, m_draft);
    if (delta > 0 && !m_rFaction.GetEconomy().CanAfford(delta))
    {
        std::ostringstream oss;
        oss << "Such a drastic change in our society would cost us " << delta
            << " energy credits to effect, but we have only "
            << m_rFaction.GetEconomy().GetEnergy() << " credits in reserve.";
        m_notice(oss.str());
        return;
    }

    Faction& rFaction = m_rFaction;
    const SocialPolicyDraft_t draft = m_draft;
    const CommittedFn onCommitted = m_onCommitted;
    auto adoptAndClose = [&rFaction, draft, onCommitted]() {
        AdoptDraft(rFaction, draft);
        onCommitted();
    };

    if (delta <= 0)
    {
        adoptAndClose();
        return;
    }

    std::ostringstream oss;
    oss << "Such a drastic change in our society will cost us " << delta
        << " energy credits to effect. We have " << m_rFaction.GetEconomy().GetEnergy()
        << " credits available.";
    m_confirm(oss.str(), adoptAndClose);
}

} // namespace ac
