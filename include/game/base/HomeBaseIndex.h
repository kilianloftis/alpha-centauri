#pragma once

#include "lib/Revision.h"
#include <vector>

namespace ac
{

class BaseManager;
class Unit;
class HomeBaseIndex;

// Holding the claim is the home-base link; many units may claim one base. Only
// HomeBaseIndex::Claim mints one, and ~HomeBaseIndex orphans any left.
class HomeBaseClaim
{
public:
    HomeBaseClaim() = default;
    ~HomeBaseClaim();

    HomeBaseClaim(HomeBaseClaim&& rOther) noexcept;
    HomeBaseClaim& operator=(HomeBaseClaim&& rOther) noexcept;
    HomeBaseClaim(const HomeBaseClaim&) = delete;
    HomeBaseClaim& operator=(const HomeBaseClaim&) = delete;

    // The home base, or nullptr for an empty claim.
    BaseManager* GetBase() const;

private:
    friend class HomeBaseIndex;
    HomeBaseClaim(HomeBaseIndex& rIndex, Unit& rUnit);

    void Release_();
    void Orphan_() noexcept;
    void MoveFrom_(HomeBaseClaim& rOther) noexcept;

    HomeBaseIndex* m_pIndex = nullptr;
};

// Which units call this base home. Changes only through Claim and release, so a destroyed base
// leaves no unit pointing at it.
class HomeBaseIndex
{
public:
    explicit HomeBaseIndex(BaseManager& rBase);
    ~HomeBaseIndex();

    // Non-movable: outstanding claims hold a pointer back to this index.
    HomeBaseIndex(const HomeBaseIndex&) = delete;
    HomeBaseIndex& operator=(const HomeBaseIndex&) = delete;

    BaseManager& GetBase() { return m_rBase; }
    const BaseManager& GetBase() const { return m_rBase; }

    // Units that currently claim this base as home (order is insertion order).
    const std::vector<Unit*>& GetUnits() const { return m_units; }

    // Register rUnit as having this base for its home. Always succeeds — multiple units may
    // share one home. The returned claim is empty only if default-constructed, never from here.
    HomeBaseClaim Claim(Unit& rUnit);

    // Bumped on every claim and release; not bumped when the index orphans claims on destroy.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    friend class HomeBaseClaim;
    void Release_(HomeBaseClaim& rClaim);
    void UpdateClaimPointer_(HomeBaseClaim* pFrom, HomeBaseClaim* pTo);

    BaseManager& m_rBase;
    // Parallel to m_units: back-pointers so moves and ~HomeBaseIndex can find live claims.
    std::vector<HomeBaseClaim*> m_claims;
    std::vector<Unit*> m_units;
    Revision m_revision;
};

} // namespace ac
