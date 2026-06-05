#include "game/faction/FactionIdentity.h"

namespace ac
{

FactionIdentity::FactionIdentity()
{
}

FactionIdentity::~FactionIdentity()
{
}

const std::string& FactionIdentity::GetName() const
{
    return m_name;
}

void FactionIdentity::SetName(const std::string& rName)
{
    m_name = rName;
}

const std::string& FactionIdentity::GetLeader() const
{
    return m_leader;
}

void FactionIdentity::SetLeader(const std::string& rLeader)
{
    m_leader = rLeader;
}

} // namespace ac
