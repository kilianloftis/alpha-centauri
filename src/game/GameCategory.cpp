#include "game/GameCategory.h"

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

std::string GameCategoryToString(GameCategory_t category)
{
    const auto name = magic_enum::enum_name(category);
    if (name.empty())
    {
        throw std::runtime_error("Unknown game category");
    }
    return ToLower_(std::string(name));
}

GameCategory_t ParseGameCategory(const std::string& category)
{
    const std::string normalized = ToLower_(category);
    for (const GameCategory_t value : magic_enum::enum_values<GameCategory_t>())
    {
        if (ToLower_(std::string(magic_enum::enum_name(value))) == normalized)
        {
            return value;
        }
    }

    throw std::runtime_error("Unknown game category: '" + category + "'");
}

GameCategory_t ParseGameCategoryField(const nlohmann::json& j, const char* key)
{
    return ParseGameCategory(j.at(key).get<std::string>());
}

} // namespace ac
