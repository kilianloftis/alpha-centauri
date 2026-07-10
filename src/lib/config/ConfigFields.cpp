#include "lib/config/ConfigFields.h"

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
    return j.value(key, id);
}

std::string ParseRequiredTech(const nlohmann::json& j)
{
    return j.value("required_tech", std::string());
}

std::vector<std::string> ParseStringArray(const nlohmann::json& j, const std::string& key)
{
    std::vector<std::string> values;
    const auto it = j.find(key);
    if (it != j.end() && it->is_array())
    {
        for (const auto& rEntry : *it)
        {
            values.push_back(rEntry.get<std::string>());
        }
    }
    return values;
}

} // namespace ConfigFields
} // namespace ac
