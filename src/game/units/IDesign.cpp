#include "game/units/IDesign.h"

#include "game/effects/ActiveEffect.h"

#include <algorithm>

namespace ac
{

bool IDesign::IsNativeLife() const
{
    return ResolveFlag(*this, RuleFlagId_t::NativeLife);
}

int IDesign::GetMovementPoints() const
{
    return ResolveStat(*this, StatId_t::Movement);
}

int IDesign::GetMineralUpkeep() const
{
    return std::max(0, ResolveStat(*this, StatId_t::MineralUpkeep));
}

} // namespace ac
