#include "game/faction/Military.h"
#include "game/units/UnitDesign.h"

namespace ac
{

Military::Military() = default;
Military::~Military() = default;

bool Military::AddDesign(std::unique_ptr<UnitDesign> pDesign)
{
    if (!pDesign) {
        return false;
    }
    
    // Check for duplicate design by ID using the map for O(1) lookup
    const std::string newDesignId = pDesign->GetId();
    if (m_designMap.find(newDesignId) != m_designMap.end()) {
        return false; // Duplicate found
    }
    
    UnitDesign* pDesignRaw = pDesign.get();
    m_designs.push_back(std::move(pDesign));
    m_designMap[newDesignId] = pDesignRaw;
    return true;
}

const std::vector<std::unique_ptr<UnitDesign>>& Military::GetDesigns() const
{
    return m_designs;
}

const UnitDesign* Military::GetDesign(const std::string& designId) const
{
    auto it = m_designMap.find(designId);
    return it != m_designMap.end() ? it->second : nullptr;
}

} // namespace ac
