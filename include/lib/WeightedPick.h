#pragma once

#include <cstddef>
#include <optional>
#include <random>

namespace ac
{

// Index i in [0, count) with probability weightAt(i) / sum(weights).
// Empty or non-positive total → nullopt (does not draw from rRng).
template <typename WeightAt>
std::optional<std::size_t> PickWeightedIndex(std::size_t count, WeightAt&& weightAt,
                                             std::mt19937& rRng)
{
    if (count == 0)
    {
        return std::nullopt;
    }

    int totalWeight = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        totalWeight += weightAt(i);
    }
    if (totalWeight <= 0)
    {
        return std::nullopt;
    }

    std::uniform_int_distribution<int> pick(1, totalWeight);
    int roll = pick(rRng);
    for (std::size_t i = 0; i < count; ++i)
    {
        roll -= weightAt(i);
        if (roll <= 0)
        {
            return i;
        }
    }
    return count - 1;
}

} // namespace ac
