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
        Faction* pFaction,
        const SocialPolicyRegistry* pPolicyRegistry,
        const SocialRatingRegistry* pRatingRegistry,
        WindowLayout_t layout
    );
    ~SocialEngineeringDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    Faction* m_pFaction = nullptr;
    const SocialPolicyRegistry* m_pPolicyRegistry = nullptr;
    const SocialRatingRegistry* m_pRatingRegistry = nullptr;
};

} // namespace ac
