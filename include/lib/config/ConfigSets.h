#pragma once

#include "lib/config/ConfigReader.h"
#include "lib/config/EnumNames.h"

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace ConfigFields
{

template <typename Level_t>
struct LevelSet_t
{
    std::string defaultId;
    std::vector<Level_t> levels;
};

// "default": id and "levels": [...], each level parsed by parseLevel(const ConfigReader&).
// Level ids are unique, and the default names one of them. Other keys are the caller's.
template <typename Level_t, typename ParseLevelFn>
LevelSet_t<Level_t> ParseLevelSet(const ConfigReader& rReader, ParseLevelFn parseLevel)
{
    LevelSet_t<Level_t> set;
    set.defaultId = rReader.NonEmptyString("default");
    rReader.NonEmptyArray("levels");
    std::unordered_set<std::string> seenIds;
    for (const ConfigReader& rLevelReader : rReader.Objects("levels"))
    {
        Level_t level = parseLevel(rLevelReader);
        if (!seenIds.insert(level.id).second)
        {
            rReader.Fail("duplicate level id '" + level.id + "'");
        }
        set.levels.push_back(std::move(level));
    }
    if (!seenIds.contains(set.defaultId))
    {
        rReader.FailKey("default", "'" + set.defaultId + "' does not match any level id");
    }
    return set;
}

// An object keyed by enumerator name with every enumerator exactly once, each entry an object
// parsed by parseEntry(const ConfigReader&, value). rWhat names the enum in errors.
template <typename Enum_t, typename Entry_t, typename ParseEntryFn>
std::array<Entry_t, magic_enum::enum_count<Enum_t>()> ParseEnumKeyedObject(
    const ConfigReader& rObject, const char* rWhat, ParseEntryFn parseEntry)
{
    std::array<Entry_t, magic_enum::enum_count<Enum_t>()> entries{};
    std::array<bool, magic_enum::enum_count<Enum_t>()> seen{};
    for (const auto& [rName, rUnused] : rObject.Json().items())
    {
        const Enum_t value = EnumFromName<Enum_t>(rName, rWhat);
        const std::size_t index = *magic_enum::enum_index(value);
        if (seen[index])
        {
            rObject.Fail(std::string("repeats ") + rWhat + " '" + rName + "'");
        }
        seen[index] = true;
        entries[index] = parseEntry(rObject.Object(rName), value);
    }
    for (const Enum_t value : magic_enum::enum_values<Enum_t>())
    {
        if (!seen[*magic_enum::enum_index(value)])
        {
            rObject.Fail(std::string("is missing ") + rWhat + " '"
                         + std::string(magic_enum::enum_name(value)) + "'");
        }
    }
    return entries;
}

} // namespace ConfigFields

} // namespace ac
