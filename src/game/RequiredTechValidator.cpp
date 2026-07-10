#include "game/RequiredTechValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/UnitSlotRegistry.h"

#include <stdexcept>

namespace ac
{

namespace
{

template <typename TConfig>
void ValidateRequiredTech_(const std::vector<TConfig>& rConfigs, const TechRegistry& rTechs,
                           const char* what)
{
    for (const TConfig& rConfig : rConfigs)
    {
        if (!rConfig.requiredTech.empty() && !rTechs.Find(rConfig.requiredTech))
        {
            throw std::runtime_error(
                std::string(what) + " '" + rConfig.id + "' has required_tech '"
                + rConfig.requiredTech + "' which is not a known tech");
        }
    }
}

} // namespace

void ValidateRequiredTechReferences(const GameDataContext& rData)
{
    if (!rData.techRegistry)
    {
        return;
    }
    const TechRegistry& rTechs = *rData.techRegistry;

    if (rData.buildingRegistry)
    {
        ValidateRequiredTech_(rData.buildingRegistry->GetAll(), rTechs, "Building");
    }
    if (rData.improvementRegistry)
    {
        ValidateRequiredTech_(rData.improvementRegistry->GetAll(), rTechs, "Improvement");
    }
    if (rData.unitComponentRegistry)
    {
        ValidateRequiredTech_(rData.unitComponentRegistry->GetAll(), rTechs, "Unit component");
    }
    if (rData.unitSlotRegistry)
    {
        ValidateRequiredTech_(rData.unitSlotRegistry->GetAll(), rTechs, "Unit slot");
    }
    if (rData.socialPolicyRegistry)
    {
        ValidateRequiredTech_(rData.socialPolicyRegistry->GetAll(), rTechs, "Social policy");
    }
    if (rData.popTypeRegistry)
    {
        ValidateRequiredTech_(rData.popTypeRegistry->GetAll(), rTechs, "Pop type");
    }
}

} // namespace ac
