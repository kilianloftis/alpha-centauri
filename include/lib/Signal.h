#pragma once

#include <functional>
#include <vector>

namespace ac
{

template<typename... Args>
class Signal
{
public:
    using Slot = std::function<void(Args...)>;

    void connect(Slot slot)
    {
        m_slots.push_back(std::move(slot));
    }

    void emit(Args... args) const
    {
        for (auto& slot : m_slots) slot(args...);
    }

private:
    std::vector<Slot> m_slots;
};

} // namespace ac