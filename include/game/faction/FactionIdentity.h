#pragma once

#include <string>

namespace ac
{

class FactionIdentity
{
public:
    FactionIdentity();
    ~FactionIdentity();

    const std::string& GetName() const;
    void SetName(const std::string& rName);

    const std::string& GetLeader() const;
    void SetLeader(const std::string& rLeader);

private:
    std::string m_name;
    std::string m_leader;
};

} // namespace ac
