#pragma once

#include "game/RequirementLists.h"
#include "lib/config/ConfigReader.h"

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

// Parses `{ "any": [...], "all": [...], "none": [...] }` at pKey. Absent key → empty.
// rAllowedKeys is the closed set (techs: any/all; buildings: any/all/none).
// parseEntry reads one object from an entry list.
template <typename Entry_t, typename ParseEntryFn>
RequirementLists_t<Entry_t> ParseRequirementLists(const ConfigReader& rReader, const char* pKey,
                                                    ParseEntryFn&& parseEntry,
                                                    std::initializer_list<const char*> rAllowedKeys)
{
    if (!rReader.Has(pKey))
    {
        return {};
    }
    const ConfigReader lists = rReader.Object(pKey);
    lists.RejectUnknownKeys(std::vector<std::string>(rAllowedKeys.begin(), rAllowedKeys.end()));

    auto parseList = [&](const char* pListKey) -> std::vector<Entry_t>
    {
        if (!lists.Has(pListKey))
        {
            return {};
        }
        std::vector<Entry_t> entries;
        for (const ConfigReader& rEntry : lists.Objects(pListKey))
        {
            entries.push_back(parseEntry(rEntry));
        }
        return entries;
    };

    RequirementLists_t<Entry_t> result;
    result.any = parseList("any");
    result.all = parseList("all");
    result.none = parseList("none");
    return result;
}

} // namespace ac
