#pragma once

#include <array>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

enum class GameCategory
{
    Build,
    Grow,
    Discover,
    Conquer
};

constexpr size_t k_GameCategoryCount = 4;

constexpr std::array<GameCategory, k_GameCategoryCount> k_AllGameCategories = {
    GameCategory::Build,
    GameCategory::Grow,
    GameCategory::Discover,
    GameCategory::Conquer,
};

std::string GameCategoryToString(GameCategory category);
GameCategory ParseGameCategory(const std::string& category);

// Required game category at j[key] (default key "category"). Named distinctly from
// ParseGameCategory(const std::string&) — a same-named overload would be ambiguous for any
// call passing a string literal, since both a std::string and a nlohmann::json can be
// constructed from one via an implicit conversion.
GameCategory ParseGameCategoryField(const nlohmann::json& j, const char* key = "category");

} // namespace ac
