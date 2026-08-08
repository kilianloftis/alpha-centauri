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
    // Dead members after the children are built, so nothing is stored: the panels own the
    // references they need.
    SocialEngineeringView(
        Faction& rFaction,
        const SocialPolicyRegistry& rPolicyRegistry,
        const SocialRatingRegistry& rRatingRegistry,
        WindowLayout_t layout
    );

    bool HandleKey(const KeyEvent_t& rEvent) override;
};

} // namespace ac
