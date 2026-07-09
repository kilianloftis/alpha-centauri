#pragma once

#include <memory>
#include <ranges>
#include <vector>

namespace ac
{

// Adapts a vector<unique_ptr<T>> into a range of T& (or const T&), so an owning container
// can be iterated by reference without exposing the unique_ptrs — callers can read and
// mutate the elements but cannot reseat, move out, or destroy them.
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
