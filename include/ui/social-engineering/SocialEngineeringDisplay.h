#pragma once

#include "ui/UIElement.h"

namespace ac
{

class Faction;
class SocialPolicyRegistry;
class SocialRatingRegistry;

class SocialEngineeringDisplay : public UIElement
{
public:
    SocialEngineeringDisplay(
        Faction& rFaction,
        const SocialPolicyRegistry& rPolicyRegistry,
        const SocialRatingRegistry& rRatingRegistry,
        WindowLayout_t layout
    );
    ~SocialEngineeringDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

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
    void DrawScores_(Graphics& rGraphics, const WindowLayout_t& rScoresList,
                     const Padding_t& rPadding) const;
    void DrawFactionBonus_(Graphics& rGraphics, const WindowLayout_t& rBonusSection,
                           const Padding_t& rPadding) const;

    Faction& m_rFaction;
    const SocialPolicyRegistry& m_rPolicyRegistry;
    const SocialRatingRegistry& m_rRatingRegistry;
};

} // namespace ac
