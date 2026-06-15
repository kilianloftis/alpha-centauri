#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "lib/LuaRuntime.h"

namespace ac
{

GameDataContext::GameDataContext() = default;
GameDataContext::~GameDataContext() = default;

} // namespace ac
