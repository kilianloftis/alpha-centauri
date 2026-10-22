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

    bool HasBuiltComponent(const std::string& componentId) const;

    // Marks rDesign's components as fielded; a NativeDesign has none.
    void RecordBuiltComponents(const IDesign& rDesign);

private:
    std::vector<std::unique_ptr<IDesign>> m_designs;
    std::unordered_set<std::string> m_builtComponentIds;
};

} // namespace ac
