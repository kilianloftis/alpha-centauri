#include "game/faction/FactionIdentity.h"

namespace ac
{

FactionIdentity::FactionIdentity(const FactionIdentityConfig& rIdentity, const LeaderConfig& rLeader)
    : m_name(rIdentity.name)
    , m_descriptiveName(rIdentity.descriptiveName)
    , m_noun(rIdentity.noun)
    , m_adjective(rIdentity.adjective)
    , m_leader(rLeader.name)
    , m_leaderTitle(rLeader.title)
{
}

FactionIdentity::~FactionIdentity()
{
}

const std::string& FactionIdentity::GetName() const
{
    return m_name;
}

const std::string& FactionIdentity::GetDescriptiveName() const
{
    return m_descriptiveName;
}

const std::string& FactionIdentity::GetNoun() const
{
    return m_noun;
}

const std::string& FactionIdentity::GetAdjective() const
{
    return m_adjective;
}

const std::string& FactionIdentity::GetLeader() const
{
    return m_leader;
}

const std::string& FactionIdentity::GetLeaderTitle() const
{
    return m_leaderTitle;
}

} // namespace ac
