#include "game/base/BaseRules.h"

#include "game/faction/Faction.h"

namespace ac
{

bool GrantsHeadquarters(const Faction& rFaction)
{
    return rFaction.GetHeadquarters() == nullptr;
}

} // namespace ac
