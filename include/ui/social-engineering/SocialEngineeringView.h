#pragma once

#include "ui/IGameView.h"

#include <string>

namespace ac
{

class Faction;
class SocialEngineeringDisplay;
class SocialPolicyRegistry;
class SocialRatingRegistry;

class SocialEngineeringView : public IGameView
{
public:
    // Stores nothing: the panels it builds hold the references they need.
    SocialEngineeringView(
        Faction& rFaction,
        const SocialPolicyRegistry& rPolicyRegistry,
        const SocialRatingRegistry& rRatingRegistry,
        WindowLayout_t layout
    );

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void Notice_(std::string message);

    SocialEngineeringDisplay* m_pDisplay = nullptr;
};

} // namespace ac
