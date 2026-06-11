#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/base/population/pop-types/PopTypeRegistry.h"
#include "game/faction/base/population/pop-types/PopCompositionConfigParser.h"
#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include "lib/LuaRuntime.h"

namespace ac
{

GameDataContext::GameDataContext() = default;
GameDataContext::~GameDataContext() = default;

} // namespace ac
