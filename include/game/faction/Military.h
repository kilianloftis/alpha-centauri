#pragma once

#include <memory>
#include <vector>

namespace ac
{

class UnitDesign;

class Military
{
public:
    Military();
    ~Military();

    void AddDesign(std::unique_ptr<UnitDesign> pDesign);
    const std::vector<std::unique_ptr<UnitDesign>>& GetDesigns() const;

private:
    std::vector<std::unique_ptr<UnitDesign>> m_designs;
};

} // namespace ac
