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

std::string_view AtrocitySeverityLabel(AtrocitySeverityId_t id)
{
    switch (id)
    {
    case AtrocitySeverityId_t::Simple:
        return "simple";
    case AtrocitySeverityId_t::Major:
        return "major";
    }
    throw std::runtime_error("Atrocity severity has no label: "
                             + std::to_string(static_cast<int>(id)));
}

const AtrocitySeverityConfig_t& AtrocitiesConfig_t::For(AtrocitySeverityId_t id) const
{
    return severities[RequireIndex_(id)];
}

} // namespace ac
