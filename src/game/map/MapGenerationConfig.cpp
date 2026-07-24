#include "game/map/MapGenerationConfig.h"

#include <magic_enum.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ac
{

namespace
{

std::string ToLower_(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

} // namespace

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
    const std::string normalized = ToLower_(value);
    for (const ErosiveForces_t level : magic_enum::enum_values<ErosiveForces_t>())
    {
        if (ToLower_(std::string(magic_enum::enum_name(level))) == normalized)
        {
            return level;
        }
    }
    throw std::runtime_error("Unknown erosive forces value: '" + value + "'");
}

} // namespace ac
