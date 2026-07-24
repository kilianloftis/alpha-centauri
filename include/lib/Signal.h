#pragma once

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace ac
{

using SlotId_t = int;

template<typename... Args>
class Signal
{
public:
    using Slot = std::function<void(Args...)>;

    Signal() = default;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = delete;
    Signal& operator=(Signal&&) = delete;

    // RAII guard: disconnects on destruction / move-assignment. Signal must outlive
    // any live ScopedConnection that still points at it.
    class ScopedConnection
    {
    public:
        ScopedConnection() = default;

        ScopedConnection(Signal* pSignal, SlotId_t id)
            : m_pSignal(pSignal)
            , m_id(id)
        {
        }

        ~ScopedConnection()
        {
            Disconnect();
        }

        ScopedConnection(const ScopedConnection&) = delete;
        ScopedConnection& operator=(const ScopedConnection&) = delete;

        ScopedConnection(ScopedConnection&& other) noexcept
            : m_pSignal(other.m_pSignal)
            , m_id(other.m_id)
        {
            other.Release_();
        }

        ScopedConnection& operator=(ScopedConnection&& other) noexcept
        {
            if (this != &other)
            {
                Disconnect();
                m_pSignal = other.m_pSignal;
                m_id = other.m_id;
                other.Release_();
            }
            return *this;
        }

        void Disconnect()
        {
            if (m_pSignal)
            {
                m_pSignal->Disconnect(m_id);
                Release_();
            }
        }

        bool IsConnected() const { return m_pSignal != nullptr; }

    private:
        void Release_()
        {
            m_pSignal = nullptr;
            m_id = 0;
        }

        Signal* m_pSignal = nullptr;
        SlotId_t m_id = 0;
    };

    SlotId_t Connect(Slot slot)
    {
        const SlotId_t id = m_nextId++;
        m_slots.emplace_back(id, std::move(slot));
        return id;
    }

    [[nodiscard]] ScopedConnection ConnectScoped(Slot slot)
    {
        return ScopedConnection(this, Connect(std::move(slot)));
    }

    void Disconnect(SlotId_t id)
    {
        m_slots.erase(
            std::remove_if(m_slots.begin(), m_slots.end(),
                [id](const auto& entry) { return entry.first == id; }),
            m_slots.end());
    }

    void Emit(Args... args) const
    {
        // Snapshot so Connect/Disconnect during dispatch cannot invalidate iteration.
        // Slots disconnected after the snapshot was taken (and not yet invoked) are skipped.
        const auto snapshot = m_slots;
        for (const auto& [id, slot] : snapshot)
        {
            if (!IsConnected_(id)) continue;
            slot(args...);
        }
    }

private:
    bool IsConnected_(SlotId_t id) const
    {
        for (const auto& [slotId, _] : m_slots)
        {
            if (slotId == id) return true;
        }
        return false;
    }

    std::vector<std::pair<SlotId_t, Slot>> m_slots;
    SlotId_t m_nextId = 1; // 0 reserved for empty ScopedConnection
};

} // namespace ac
