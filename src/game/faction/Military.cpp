#include "game/faction/Military.h"

#include "game/units/IDesign.h"
#include "game/units/UnitDesign.h"

namespace ac
{

Military::Military() = default;
Military::~Military() = default;

bool Military::AddDesign(std::unique_ptr<IDesign> pDesign)
{
    if (!pDesign)
    {
        return false;
    }

    const std::string newDesignId = pDesign->GetId();
    for (const std::unique_ptr<IDesign>& rExisting : m_designs)
    {
        if (rExisting->GetId() == newDesignId)
        {
            return false;
        }
    }

    m_designs.push_back(std::move(pDesign));
    return true;
}

const std::vector<std::unique_ptr<IDesign>>& Military::GetDesigns() const
{
    return m_designs;
}

const IDesign* Military::GetDesign(const std::string& designId) const
{
    for (const std::unique_ptr<IDesign>& rDesign : m_designs)
    {
        if (rDesign->GetId() == designId)
        {
            return rDesign.get();
        }
    }
    return nullptr;
}

bool Military::IsPrototype(const IDesign& rDesign) const
{
    const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(&rDesign);
    if (!pUnitDesign)
    {
        return false;
    }
    for (const UnitComponentConfig_t* pComp : pUnitDesign->GetComponents())
    {
        if (pComp && !m_builtComponentIds.contains(pComp->id))
        {
            return true;
        }
    }
    return false;
}

void Military::RecordBuiltComponents(const IDesign& rDesign)
{
    const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(&rDesign);
    if (!pUnitDesign)
    {
        return;
    }
    for (const UnitComponentConfig_t* pComp : pUnitDesign->GetComponents())
    {
        if (pComp)
        {
            m_builtComponentIds.insert(pComp->id);
        }
    }
}

} // namespace ac
