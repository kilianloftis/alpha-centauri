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
    Faction& m_rFaction;
    const SocialPolicyRegistry& m_rPolicyRegistry;
    const SocialRatingRegistry& m_rRatingRegistry;
};

} // namespace ac
