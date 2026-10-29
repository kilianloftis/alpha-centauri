#include "game/atrocities/AtrocityConfig.h"

#include <magic_enum.hpp>

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

std::size_t RequireIndex_(AtrocitySeverityId_t id)
{
    const auto index = magic_enum::enum_index(id);
    if (!index)
    {
        throw std::runtime_error("Unknown atrocity severity: "
                                 + std::to_string(static_cast<int>(id)));
    }
    return *index;
}

} // namespace

const AtrocitySeverityConfig_t& AtrocitiesConfig_t::For(AtrocitySeverityId_t id) const
{
    return severities[RequireIndex_(id)];
}

} // namespace ac
