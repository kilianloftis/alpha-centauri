#pragma once

#include "game/faction/FactionConfig.h"
#include <string>

namespace ac
{

class FactionIdentity
{
public:
    FactionIdentity(const FactionIdentityConfig& rIdentity, const LeaderConfig& rLeader);
    ~FactionIdentity();

    const std::string& GetName() const;
    const std::string& GetDescriptiveName() const;
    const std::string& GetNoun() const;
    const std::string& GetAdjective() const;
    const std::string& GetLeader() const;
    const std::string& GetLeaderTitle() const;

private:
    std::string m_name;
    std::string m_descriptiveName;
    std::string m_noun;
    std::string m_adjective;
    std::string m_leader;
    std::string m_leaderTitle;
};

} // namespace ac
