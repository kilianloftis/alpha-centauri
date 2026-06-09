#include "game/faction/population/PopFactory.h"
#include "game/faction/population/Pop.h"
#include "game/faction/population/PopTypeRegistry.h"
#include <iostream>

namespace ac
{

PopFactory::PopFactory()
{
}

PopFactory::~PopFactory()
{
}

void PopFactory::SetRegistry(const PopTypeRegistry* pRegistry)
{
    m_pRegistry = pRegistry;
}

std::unique_ptr<Pop> PopFactory::CreatePop(const std::string& typeId) const
{
    if (!m_pRegistry)
    {
        throw std::runtime_error("PopFactory has no registry");
    }

    const PopTypeConfig* pConfig = m_pRegistry->Find(typeId);
    if (!pConfig)
    {
        throw std::runtime_error("Unknown pop type '" + typeId + "'");
    }

    return std::make_unique<Pop>(*pConfig);
}

} // namespace ac
