#pragma once

#include <cstdint>

namespace ac
{

// Change counter for pull-based caches: owners bump it in every mutator, and consumers rebuild
// when a revision they recorded has moved.
class Revision
{
public:
    void Bump() { ++m_value; }
    uint64_t Get() const { return m_value; }

private:
    uint64_t m_value = 0;
};

} // namespace ac
