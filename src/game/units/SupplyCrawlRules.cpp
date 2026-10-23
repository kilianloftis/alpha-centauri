#include "game/units/SupplyCrawlRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/units/Unit.h"

#include <stdexcept>

namespace ac
{

bool CanStartSupplyCrawl(const Unit& rUnit, StatId_t resource)
{
    if (!IsCrawlResource(resource))
    {
        throw std::runtime_error(
            "CanStartSupplyCrawl: resource must be nutrients, minerals, or energy");
    }
    return ResolveFlag(rUnit, RuleFlagId_t::SupplyCrawl) && rUnit.GetHomeBase() != nullptr;
}

} // namespace ac
