#include "game/faction/base/HomeBaseIndex.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/Unit.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

HomeBaseClaim::HomeBaseClaim(HomeBaseIndex& rIndex, Unit& rUnit)
    : m_pIndex(&rIndex)
    , m_pUnit(&rUnit)
{
    m_pIndex->m_claims.push_back(this);
    m_pIndex->m_units.push_back(&rUnit);
}

HomeBaseClaim::~HomeBaseClaim()
{
    Release_();
}

HomeBaseClaim::HomeBaseClaim(HomeBaseClaim&& rOther) noexcept
{
    MoveFrom_(rOther);
}

HomeBaseClaim& HomeBaseClaim::operator=(HomeBaseClaim&& rOther) noexcept
{
    if (this != &rOther)
    {
        Release_();
        MoveFrom_(rOther);
    }
    return *this;
}

BaseManager* HomeBaseClaim::GetBase() const
{
    return m_pIndex ? &m_pIndex->GetBase() : nullptr;
}

void HomeBaseClaim::MoveFrom_(HomeBaseClaim& rOther) noexcept
{
    m_pIndex = rOther.m_pIndex;
    m_pUnit = rOther.m_pUnit;
    if (m_pIndex)
    {
        m_pIndex->UpdateClaimPointer_(&rOther, this);
    }
    rOther.m_pIndex = nullptr;
    rOther.m_pUnit = nullptr;
}

void HomeBaseClaim::Release_()
{
    if (m_pIndex)
    {
        m_pIndex->Release_(*this);
    }
    m_pIndex = nullptr;
    m_pUnit = nullptr;
}

void HomeBaseClaim::Orphan_() noexcept
{
    m_pIndex = nullptr;
    m_pUnit = nullptr;
}

HomeBaseIndex::HomeBaseIndex(BaseManager& rBase)
    : m_rBase(rBase)
{
}

HomeBaseIndex::~HomeBaseIndex()
{
    // Base is going away: empty every claim in place so units never keep a dangling base.
    // Do not call Release_ — the vectors are about to die with us.
    for (HomeBaseClaim* pClaim : m_claims)
    {
        pClaim->Orphan_();
    }
    m_claims.clear();
    m_units.clear();
}

HomeBaseClaim HomeBaseIndex::Claim(Unit& rUnit)
{
    m_revision.Bump();
    // The constructor registers the claim in m_claims / m_units.
    return HomeBaseClaim(*this, rUnit);
}

void HomeBaseIndex::Release_(HomeBaseClaim& rClaim)
{
    auto claimIt = std::find(m_claims.begin(), m_claims.end(), &rClaim);
    if (claimIt == m_claims.end())
    {
        throw std::logic_error("HomeBaseIndex: released a claim that was not registered");
    }
    const auto idx = static_cast<std::size_t>(claimIt - m_claims.begin());
    m_claims.erase(claimIt);
    m_units.erase(m_units.begin() + static_cast<std::ptrdiff_t>(idx));
    m_revision.Bump();
}

void HomeBaseIndex::UpdateClaimPointer_(HomeBaseClaim* pFrom, HomeBaseClaim* pTo)
{
    auto it = std::find(m_claims.begin(), m_claims.end(), pFrom);
    if (it == m_claims.end())
    {
        throw std::logic_error("HomeBaseIndex: moved a claim that was not registered");
    }
    *it = pTo;
}

} // namespace ac
