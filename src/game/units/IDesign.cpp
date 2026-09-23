#include "game/units/IDesign.h"

#include "game/effects/ActiveEffect.h"

#include <algorithm>

namespace ac
{

int IDesign::GetStat(StatId_t statId) const
{
    return ResolveStat(*this, statId);
}

int IDesign::GetStat(StatId_t statId, const EffectContext_t& rCtx) const
{
    return ResolveStat(*this, statId, rCtx);
}

bool IDesign::GetFlag(RuleFlagId_t flagId) const
{
    return ResolveFlag(*this, flagId);
}

int IDesign::GetMovementPoints() const
{
    return GetStat(StatId_t::Movement);
}

int IDesign::GetMineralUpkeep() const
{
    return std::max(0, GetStat(StatId_t::MineralUpkeep));
}

} // namespace ac
