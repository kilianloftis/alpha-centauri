#pragma once

#include <array>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

enum class GameCategory_t
{
    Build,
    Grow,
    Discover,
    Conquer
};

constexpr size_t k_GameCategoryCount = 4;

constexpr std::array<GameCategory_t, k_GameCategoryCount> k_AllGameCategories = {
    GameCategory_t::Build,
    GameCategory_t::Grow,
    GameCategory_t::Discover,
    GameCategory_t::Conquer,
};

std::string GameCategoryToString(GameCategory_t category);
GameCategory_t ParseGameCategory(const std::string& category);

// Required game category at j[key] (default key "category"). Named distinctly from
// ParseGameCategory(const std::string&) — a same-named overload would be ambiguous for any
// call passing a string literal, since both a std::string and a nlohmann::json can be
// constructed from one via an implicit conversion.
GameCategory_t ParseGameCategoryField(const nlohmann::json& j, const char* key = "category");

} // namespace ac
