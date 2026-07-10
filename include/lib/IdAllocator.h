#pragma once

namespace ac
{

// Hands out unique, monotonically increasing integer IDs starting at 1. One instance per
// ID namespace (e.g. GameState owns one for faction IDs and one for base IDs) so allocation
// has a single owner instead of being re-derived as a local counter at each call site.
class IdAllocator
{
public:
    int Allocate() { return m_nextId++; }

private:
    int m_nextId = 1;
};

} // namespace ac
