#pragma once

#include <array>
#include <cstddef>
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

} // namespace ac
