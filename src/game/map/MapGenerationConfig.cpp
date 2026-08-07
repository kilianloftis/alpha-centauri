#include "game/map/MapGenerationConfig.h"

#include "lib/config/EnumNames.h"

#include <magic_enum.hpp>
#include <stdexcept>

namespace ac
{

std::string ToString(ErosiveForces_t erosiveForces)
{
    const auto name = magic_enum::enum_name(erosiveForces);
    if (name.empty())
    {
        throw std::runtime_error("Unknown erosive forces value");
    }
    return std::string(name);
}

ErosiveForces_t ParseErosiveForces(const std::string& value)
{
    return EnumFromName<ErosiveForces_t>(value, "erosive forces value");
}

} // namespace ac
