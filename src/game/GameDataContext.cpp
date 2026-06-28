#include "game/GameDataContext.h"
#include "game/faction/base/production/ProductionCostConfig.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechCostCalculator.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "lib/LuaRuntime.h"

namespace ac
{

GameDataContext::GameDataContext() = default;
GameDataContext::~GameDataContext() = default;

} // namespace ac
