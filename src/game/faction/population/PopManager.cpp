#include "game/faction/population/PopManager.h"
#include "game/faction/population/Pop.h"

namespace ac
{

PopManager::PopManager()
{
}

PopManager::~PopManager()
{
}

std::unique_ptr<Pop> PopManager::CreatePop()
{
    // TODO: Add logic to determine pop type (worker, talent, drone, specialist)
    // For now, all pops are workers
    return std::make_unique<WorkerPop>();
}

} // namespace ac
