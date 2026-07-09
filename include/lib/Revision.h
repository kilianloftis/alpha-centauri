#pragma once

#include <cstdint>

namespace ac
{

// Monotonic change counter for pull-based cache invalidation.
// A class that owns derived-state inputs bumps its own Revision in every mutator;
// consumers memoize derived state together with the revision values it was built
// from (see Faction::CollectPoolRevisions_) and rebuild when any of them changed.
class Revision
{
public:
    void Bump() { ++m_value; }
    uint64_t Get() const { return m_value; }

private:
    uint64_t m_value = 0;
};

} // namespace ac
