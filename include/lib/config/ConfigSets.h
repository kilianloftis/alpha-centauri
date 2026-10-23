#pragma once

#include "lib/config/ConfigFields.h"
#include "lib/config/EnumNames.h"

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <stdexcept>
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

// {"default": id, "levels": [...]}, each level parsed by parseLevel. Level ids are unique, and
// the default names one of them.
template <typename Level_t, typename ParseLevelFn>
LevelSet_t<Level_t> ParseLevelSet(const nlohmann::json& rJson, const std::string& rContext,
                                  ParseLevelFn parseLevel)
{
    RejectUnknownKeys(rJson, {"default", "levels"}, rContext);
    if (!rJson.contains("default") || !rJson.at("default").is_string()
        || rJson.at("default").get<std::string>().empty())
    {
        throw std::runtime_error(rContext + ": 'default' must be a non-empty string");
    }
    if (!rJson.contains("levels") || !rJson.at("levels").is_array() || rJson.at("levels").empty())
    {
        throw std::runtime_error(rContext + ": 'levels' must be a non-empty array");
    }
    LevelSet_t<Level_t> set;
    set.defaultId = rJson.at("default").get<std::string>();
    std::unordered_set<std::string> seenIds;
    for (const nlohmann::json& rLevelJson : rJson.at("levels"))
    {
        Level_t level = parseLevel(rLevelJson);
        if (!seenIds.insert(level.id).second)
        {
            throw std::runtime_error(rContext + ": duplicate level id '" + level.id + "'");
        }
        set.levels.push_back(std::move(level));
    }
    if (!seenIds.contains(set.defaultId))
    {
        throw std::runtime_error(rContext + ": default '" + set.defaultId
                                 + "' does not match any level id");
    }
    return set;
}

// An object keyed by enumerator name with every enumerator exactly once, each entry parsed by
// parseEntry(rEntry, value, rName). rWhat names the enum in errors.
template <typename Enum_t, typename Entry_t, typename ParseEntryFn>
std::array<Entry_t, magic_enum::enum_count<Enum_t>()> ParseEnumKeyedObject(
    const nlohmann::json& rObject, const char* rWhat, const std::string& rContext,
    ParseEntryFn parseEntry)
{
    std::array<Entry_t, magic_enum::enum_count<Enum_t>()> entries{};
    std::array<bool, magic_enum::enum_count<Enum_t>()> seen{};
    for (const auto& [rName, rEntry] : rObject.items())
    {
        const Enum_t value = EnumFromName<Enum_t>(rName, rWhat);
        const std::size_t index = *magic_enum::enum_index(value);
        if (seen[index])
        {
            throw std::runtime_error(rContext + " repeats " + rWhat + " '" + rName + "'");
        }
        seen[index] = true;
        entries[index] = parseEntry(rEntry, value, rName);
    }
    for (const Enum_t value : magic_enum::enum_values<Enum_t>())
    {
        if (!seen[*magic_enum::enum_index(value)])
        {
            throw std::runtime_error(rContext + " is missing " + rWhat + " '"
                                     + std::string(magic_enum::enum_name(value)) + "'");
        }
    }
    return entries;
}

} // namespace ConfigFields

} // namespace ac
