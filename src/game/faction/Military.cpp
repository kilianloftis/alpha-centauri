#include "game/faction/Military.h"
#include "game/units/UnitDesign.h"

namespace ac
{

Military::Military() = default;
Military::~Military() = default;

bool Military::AddDesign(std::unique_ptr<UnitDesign> pDesign)
{
    if (!pDesign)
    {
        return false;
    }

    const std::string newDesignId = pDesign->GetId();
    for (const std::unique_ptr<UnitDesign>& rExisting : m_designs)
    {
        if (rExisting->GetId() == newDesignId)
        {
            return false;
        }
    }

    m_designs.push_back(std::move(pDesign));
    return true;
}

const std::vector<std::unique_ptr<UnitDesign>>& Military::GetDesigns() const
{
    return m_designs;
}

const UnitDesign* Military::GetDesign(const std::string& designId) const
{
    for (const std::unique_ptr<UnitDesign>& rDesign : m_designs)
    {
        if (rDesign->GetId() == designId)
        {
            return rDesign.get();
        }
    }
    return nullptr;
}

bool Military::IsPrototype(const UnitDesign& rDesign) const
{
    for (const UnitComponentConfig_t* pComp : rDesign.GetComponents())
    {
        if (pComp && !m_builtComponentIds.contains(pComp->id))
        {
            return true;
        }
    }
    return false;
}

void Military::RecordBuiltComponents(const UnitDesign& rDesign)
{
    for (const UnitComponentConfig_t* pComp : rDesign.GetComponents())
    {
        if (pComp)
        {
            m_builtComponentIds.insert(pComp->id);
        }
    }
}

bool Military::HasBuiltComponent(const std::string& rComponentId) const
{
    return m_builtComponentIds.contains(rComponentId);
}

} // namespace ac
