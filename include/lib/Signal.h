#pragma once

#include <algorithm>
#include <functional>
#include <memory>
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

    // Disconnects on destruction. The Signal may die first: the guard holds a weak token, and then
    // goes inert.
    class ScopedConnection
    {
    public:
        ScopedConnection() = default;

        ~ScopedConnection()
        {
            Disconnect();
        }

        ScopedConnection(const ScopedConnection&) = delete;
        ScopedConnection& operator=(const ScopedConnection&) = delete;

        ScopedConnection(ScopedConnection&& other) noexcept
            : m_pSignal(other.m_pSignal)
            , m_id(other.m_id)
            , m_aliveToken(std::move(other.m_aliveToken))
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
                m_aliveToken = std::move(other.m_aliveToken);
                other.Release_();
            }
            return *this;
        }

        void Disconnect()
        {
            // An expired token means the Signal is already destroyed: there is no slot list
            // left to unregister from, and m_pSignal must not be dereferenced.
            if (m_pSignal && !m_aliveToken.expired())
            {
                m_pSignal->Disconnect(m_id);
            }
            Release_();
        }

        bool IsConnected() const { return m_pSignal != nullptr && !m_aliveToken.expired(); }

    private:
        friend class Signal;

        ScopedConnection(Signal* pSignal, SlotId_t id, std::weak_ptr<const void> aliveToken)
            : m_pSignal(pSignal)
            , m_id(id)
            , m_aliveToken(std::move(aliveToken))
        {
        }

        void Release_()
        {
            m_pSignal = nullptr;
            m_id = 0;
            m_aliveToken.reset();
        }

        Signal* m_pSignal = nullptr;
        SlotId_t m_id = 0;
        std::weak_ptr<const void> m_aliveToken;
    };

    SlotId_t Connect(Slot slot)
    {
        const SlotId_t id = m_nextId++;
        m_slots.emplace_back(id, std::move(slot));
        return id;
    }

    [[nodiscard]] ScopedConnection ConnectScoped(Slot slot)
    {
        if (!m_pAliveToken)
        {
            // Created on first use so signals nobody scopes a connection to pay nothing.
            m_pAliveToken = std::make_shared<const char>('\0');
        }
        const SlotId_t id = Connect(std::move(slot));
        return ScopedConnection(this, id, m_pAliveToken);
    }

    void Disconnect(SlotId_t id)
    {
        std::erase_if(m_slots, [id](const auto& entry) { return entry.first == id; });
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
    // Handed to every ScopedConnection as a weak_ptr; expires when this Signal is destroyed,
    // which is how an outliving connection learns not to touch m_slots. Never dereferenced.
    std::shared_ptr<const void> m_pAliveToken;
};

} // namespace ac
