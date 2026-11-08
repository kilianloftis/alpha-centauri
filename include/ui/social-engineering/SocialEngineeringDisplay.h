#pragma once

#include "game/social-engineering/SocialPolicyDraft.h"
#include "ui/UIElement.h"

#include <functional>
#include <string>

namespace ac
{

class Faction;
class SocialPolicyRegistry;
struct SocialPolicyConfig_t;
class SocialRatingRegistry;

class SocialEngineeringDisplay : public UIElement
{
public:
    using ConfirmFn = std::function<void(std::string message, std::function<void()> onOk)>;
    using NoticeFn = std::function<void(std::string message)>;
    using CommittedFn = std::function<void()>;

    SocialEngineeringDisplay(
        Faction& rFaction,
        const SocialPolicyRegistry& rPolicyRegistry,
        const SocialRatingRegistry& rRatingRegistry,
        WindowLayout_t layout,
        ConfirmFn confirm,
        NoticeFn notice,
        CommittedFn onCommitted
    );
    ~SocialEngineeringDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    // OK: pay/refund upheaval for the draft, write pending, then onCommitted.
    void TryCommitDraft();

private:
    // Text inset from a panel's left and top edges, shared by the grid and the scores.
    struct Padding_t
    {
        float horizontal;
        float vertical;
    };

    unsigned int FontSize_(float heightRatio) const;
    void DrawFrame_(Graphics& rGraphics) const;
    void DrawCategory_(Graphics& rGraphics, size_t categoryIndex,
                       const WindowLayout_t& rPolicyGrid, const Padding_t& rPadding) const;
    // In the active colour, with a border, when rPolicy is the draft selection.
    void DrawPolicyCell_(Graphics& rGraphics, const WindowLayout_t& rCell,
                         const SocialPolicyConfig_t& rPolicy,
                         const SocialPolicyConfig_t& rDraftPolicy, float horizontalPadding,
                         float nameRowHeight) const;
    void DrawScores_(Graphics& rGraphics, const WindowLayout_t& rScoresList,
                     const Padding_t& rPadding) const;
    void DrawFactionBonus_(Graphics& rGraphics, const WindowLayout_t& rBonusSection,
                           const Padding_t& rPadding) const;
    WindowLayout_t OkButtonLayout_(const WindowLayout_t& rScoresList) const;
    void SelectDraftPolicy_(const SocialPolicyConfig_t& rPolicy);

    Faction& m_rFaction;
    const SocialPolicyRegistry& m_rPolicyRegistry;
    const SocialRatingRegistry& m_rRatingRegistry;
    ConfirmFn m_confirm;
    NoticeFn m_notice;
    CommittedFn m_onCommitted;
    SocialPolicyDraft_t m_draft;
};

} // namespace ac
