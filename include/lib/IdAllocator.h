#pragma once

namespace ac
{

// Unique, increasing ids from 1; one instance per id namespace.
class IdAllocator
{
public:
    int Allocate() { return m_nextId++; }

private:
    int m_nextId = 1;
};

} // namespace ac
