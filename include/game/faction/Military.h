#pragma once

#include <memory>
#include <string>
#include <unordered_set>
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

    // True when any filled component on rDesign has never been fielded by this faction.
    // Several unknown components still count as a single prototype.
    bool IsPrototype(const UnitDesign& rDesign) const;

    // Mark every filled component on rDesign as fielded. Idempotent. Called from
    // UnitManager::CreateUnit so starting units and produced units share one ledger.
    void RecordBuiltComponents(const UnitDesign& rDesign);

private:
    std::vector<std::unique_ptr<UnitDesign>> m_designs;
    std::unordered_set<std::string> m_builtComponentIds;
};

} // namespace ac
