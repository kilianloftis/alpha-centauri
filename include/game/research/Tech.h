#pragma once

#include <string>
#include <vector>

namespace ac
{

using TechId = int;

class Tech
{
public:
    Tech();
    Tech(TechId id, std::string name, std::string description);
    ~Tech();

    TechId GetId() const;
    const std::string& GetName() const;
    const std::string& GetDescription() const;

    void AddPrerequisite(TechId techId);
    const std::vector<TechId>& GetPrerequisites() const;
    bool HasPrerequisite(TechId techId) const;

    void SetBaseCost(int cost);
    int GetBaseCost() const;

private:
    TechId m_id;
    std::string m_name;
    std::string m_description;
    std::vector<TechId> m_prerequisites;
    int m_baseCost;
};

} // namespace ac
