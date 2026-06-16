#pragma once

#include <string>
#include <vector>

namespace ac
{

struct TechConfig;
using TechId = std::string;

class Tech
{
public:
    Tech();
    Tech(const TechConfig& rConfig);
    ~Tech();

    TechId GetId() const;
    const std::string& GetName() const;
    const std::string& GetDescription() const;
    const std::string& GetCategory() const;

    void AddPrerequisite(TechId techId);
    const std::vector<TechId>& GetPrerequisites() const;
    bool HasPrerequisite(TechId techId) const;

    void SetBaseCost(int cost);
    int GetBaseCost() const;

private:
    TechId m_id;
    std::string m_name;
    std::string m_description;
    std::string m_category;
    std::vector<TechId> m_prerequisites;
    int m_baseCost;
};

} // namespace ac
