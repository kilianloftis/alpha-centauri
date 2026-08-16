#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace ac
{

// Closed set of IConstructable types. JSON wire names are the snake_case spellings below.
// production.json kinds looks these up; a kind with no hurry/scrap entry cannot use that
// mechanic.
enum class ConstructableKind_t
{
    Building,
    SecretProject,
    Unit,
    Stockpile,
};

inline std::string_view ConstructableKindToString(ConstructableKind_t kind)
{
    switch (kind)
    {
    case ConstructableKind_t::Building:
        return "building";
    case ConstructableKind_t::SecretProject:
        return "secret_project";
    case ConstructableKind_t::Unit:
        return "unit";
    case ConstructableKind_t::Stockpile:
        return "stockpile";
    }
    throw std::logic_error("ConstructableKindToString: unhandled enumerator");
}

inline ConstructableKind_t ParseConstructableKind(const std::string& rKind)
{
    if (rKind == "building")
    {
        return ConstructableKind_t::Building;
    }
    if (rKind == "secret_project")
    {
        return ConstructableKind_t::SecretProject;
    }
    if (rKind == "unit")
    {
        return ConstructableKind_t::Unit;
    }
    if (rKind == "stockpile")
    {
        return ConstructableKind_t::Stockpile;
    }
    throw std::runtime_error("Unknown constructable kind: '" + rKind + "'");
}

} // namespace ac
