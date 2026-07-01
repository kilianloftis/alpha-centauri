#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

namespace ac
{

class UnitDesign;

class Military
{
public:
    Military();
    ~Military();

    bool AddDesign(std::unique_ptr<UnitDesign> pDesign);
    const std::vector<std::unique_ptr<UnitDesign>>& GetDesigns() const;
    const UnitDesign* GetDesign(const std::string& designId) const;

private:
    std::vector<std::unique_ptr<UnitDesign>> m_designs;
    std::unordered_map<std::string, UnitDesign*> m_designMap;
};

} // namespace ac
