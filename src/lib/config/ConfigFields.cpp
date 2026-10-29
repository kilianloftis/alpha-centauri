#include "lib/config/ConfigFields.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{
namespace ConfigFields
{

std::string ParseId(const nlohmann::json& j)
{
    return j.at("id").get<std::string>();
}

std::string ParseName(const nlohmann::json& j, const std::string& id, const char* key)
{
    if (!j.contains(key))
    {
        throw std::runtime_error("Config entry '" + id + "' is missing '" + key + "'");
    }
    return j.at(key).get<std::string>();
}

std::string ParseRequiredTech(const nlohmann::json& j)
{
    return j.value("required_tech", std::string());
}

std::vector<std::string> ParseStringArray(const nlohmann::json& j, const std::string& key)
{
    std::vector<std::string> values;
    const auto it = j.find(key);
    if (it == j.end())
    {
        return values;
    }
    // Names the owning entry where there is one, so a modder does not have to bisect the file.
    const auto fail = [&j, &key](const std::string& rMessage) {
        std::string where = "Field '" + key + "'";
        const auto idIt = j.find("id");
        if (idIt != j.end() && idIt->is_string())
        {
            where += " on '" + idIt->get<std::string>() + "'";
        }
        throw std::runtime_error(where + " " + rMessage);
    };

    if (!it->is_array())
    {
        fail("must be an array of strings");
    }
    for (const auto& rEntry : *it)
    {
        if (!rEntry.is_string())
        {
            fail("must contain only strings");
        }
        values.push_back(rEntry.get<std::string>());
    }
    return values;
}

int RequireInt(const nlohmann::json& j, const std::string& key, const std::string& rContext)
{
    if (!j.contains(key) || !j.at(key).is_number_integer())
    {
        throw std::runtime_error(rContext + ": '" + key + "' must be an integer");
    }
    return j.at(key).get<int>();
}

int RequireIntAtLeast(const nlohmann::json& j, const std::string& key, int minimum,
                      const std::string& rContext)
{
    const int value = RequireInt(j, key, rContext);
    if (value < minimum)
    {
        throw std::runtime_error(rContext + ": '" + key + "' must be an integer >= "
                                 + std::to_string(minimum));
    }
    return value;
}

void RejectUnknownKeys(const nlohmann::json& j, const std::vector<std::string>& rKnown,
                       const std::string& rContext)
{
    for (const auto& [rKey, rUnused] : j.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(rContext + ": unknown key '" + rKey + "'");
        }
    }
}

} // namespace ConfigFields
} // namespace ac
