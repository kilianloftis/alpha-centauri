#pragma once

#include "lib/Revision.h"
#include <vector>

namespace ac
{

class BaseManager;
class Unit;
class HomeBaseIndex;

// Move-only handle to a unit's home-base registration in HomeBaseIndex. Holding the claim
// IS the home-base link: GetBase() is the home base while the claim is live, and destruction /
// overwrite / clear releases the unit from that base's index. Unlike WorkedTileClaim there is
// no exclusivity — many units may claim the same base. Only HomeBaseIndex::Claim can create a
// non-empty claim. The index (owned by BaseManager) must outlive its claims, or destroy them
// first: ~HomeBaseIndex orphans every outstanding claim so units never keep a dangling base.
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
    Unit* m_pUnit = nullptr;
};

// Per-base owner of "which units call this base home". Owned by BaseManager. Mutation happens
// exclusively through Claim and claim release/orphan, so a destroyed base can never leave
// units pointing at it — ~HomeBaseIndex clears every claim before the BaseManager is gone.
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
