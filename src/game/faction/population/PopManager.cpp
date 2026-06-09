#include "game/faction/population/PopManager.h"
#include "game/faction/population/Pop.h"
#include "game/faction/population/PopTypeRegistry.h"
#include <iostream>

namespace ac
{

PopManager::PopManager()
{
}

PopManager::~PopManager()
{
}

void PopManager::SetRegistry(const PopTypeRegistry* pRegistry)
{
    m_pRegistry = pRegistry;
}

std::unique_ptr<Pop> PopManager::CreatePop(const std::string& typeId) const
{
    if (!m_pRegistry)
    {
        std::cout << "Warning: PopManager has no registry; cannot create pop '" << typeId << "'\n";
        return nullptr;
    }

    const PopTypeConfig* pConfig = m_pRegistry->Find(typeId);
    if (!pConfig)
    {
        std::cout << "Warning: Unknown pop type '" << typeId << "'\n";
        return nullptr;
    }

    return std::make_unique<Pop>(*pConfig);
}

} // namespace ac
