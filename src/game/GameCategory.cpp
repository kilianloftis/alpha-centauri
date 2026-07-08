#include "game/GameCategory.h"

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

std::string GameCategoryToString(GameCategory category)
{
    switch (category)
    {
    case GameCategory::Build:    return "build";
    case GameCategory::Grow:     return "grow";
    case GameCategory::Discover: return "discover";
    case GameCategory::Conquer:  return "conquer";
    }

    throw std::runtime_error("Unknown game category");
}

GameCategory ParseGameCategory(const std::string& category)
{
    const std::string normalized = ToLower_(category);
    if (normalized == "build")    return GameCategory::Build;
    if (normalized == "grow")     return GameCategory::Grow;
    if (normalized == "discover") return GameCategory::Discover;
    if (normalized == "conquer")  return GameCategory::Conquer;

    throw std::runtime_error("Unknown game category: '" + category + "'");
}

} // namespace ac
