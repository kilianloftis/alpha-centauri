#pragma once

#include <memory>
#include <ranges>
#include <vector>

namespace ac
{

// A vector<unique_ptr<T>> as a range of T&: callers use the elements but cannot reseat, move
// or destroy them.
template <typename T>
auto DerefView(std::vector<std::unique_ptr<T>>& rVec)
{
    return rVec | std::views::transform([](const std::unique_ptr<T>& rPtr) -> T& { return *rPtr; });
}

template <typename T>
auto DerefView(const std::vector<std::unique_ptr<T>>& rVec)
{
    return rVec | std::views::transform([](const std::unique_ptr<T>& rPtr) -> const T& { return *rPtr; });
}

} // namespace ac
