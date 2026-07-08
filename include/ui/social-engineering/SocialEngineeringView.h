#pragma once

#include "ui/IGameView.h"

namespace ac
{

class Faction;
class SocialPolicyRegistry;
class SocialRatingRegistry;

class SocialEngineeringView : public IGameView
{
public:
    SocialEngineeringView(
        Faction* pFaction,
        const SocialPolicyRegistry* pPolicyRegistry,
        const SocialRatingRegistry* pRatingRegistry,
        WindowLayout_t layout
    );

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    Faction* m_pFaction = nullptr;
    const SocialPolicyRegistry* m_pPolicyRegistry = nullptr;
    const SocialRatingRegistry* m_pRatingRegistry = nullptr;
};

} // namespace ac
