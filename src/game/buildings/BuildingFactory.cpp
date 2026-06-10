#include "game/buildings/BuildingFactory.h"
#include "game/buildings/Building.h"
#include "game/buildings/BuildingRegistry.h"
#include <stdexcept>

namespace ac
{

BuildingFactory::BuildingFactory()
{
}

BuildingFactory::~BuildingFactory()
{
}

void BuildingFactory::SetRegistry(const BuildingRegistry* pRegistry)
{
    m_pRegistry = pRegistry;
}

std::unique_ptr<Building> BuildingFactory::CreateBuilding(const std::string& buildingId) const
{
    if (!m_pRegistry)
    {
        throw std::runtime_error("BuildingFactory has no registry");
    }

    const BuildingConfig* pConfig = m_pRegistry->Find(buildingId);
    if (!pConfig)
    {
        throw std::runtime_error("Unknown building id '" + buildingId + "'");
    }

    return std::make_unique<Building>(*pConfig);
}

} // namespace ac
