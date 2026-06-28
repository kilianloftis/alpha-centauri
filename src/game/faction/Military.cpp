#include "game/faction/Military.h"
#include "game/units/UnitDesign.h"

namespace ac
{

Military::Military() = default;
Military::~Military() = default;

void Military::AddDesign(std::unique_ptr<UnitDesign> pDesign)
{
    m_designs.push_back(std::move(pDesign));
}

const std::vector<std::unique_ptr<UnitDesign>>& Military::GetDesigns() const
{
    return m_designs;
}

} // namespace ac
