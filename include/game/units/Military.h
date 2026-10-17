#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class IDesign;
class UnitDesign;

class Military
{
public:
    Military();
    ~Military();

    bool AddDesign(std::unique_ptr<IDesign> pDesign);
    const std::vector<std::unique_ptr<IDesign>>& GetDesigns() const;
    const IDesign* GetDesign(const std::string& designId) const;

    // True when any filled component on a UnitDesign has never been fielded by this faction.
    // NativeDesign is never a prototype. Several unknown components still count as one prototype.
    bool IsPrototype(const IDesign& rDesign) const;

    // Mark every filled component on a UnitDesign as fielded. No-op for NativeDesign.
    // Idempotent. Called from UnitManager::CreateUnit.
    void RecordBuiltComponents(const IDesign& rDesign);

private:
    std::vector<std::unique_ptr<IDesign>> m_designs;
    std::unordered_set<std::string> m_builtComponentIds;
};

} // namespace ac
