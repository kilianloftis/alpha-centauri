#include "game/RequiredTechValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfig.h"
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
#include "game/units/ProbeActionConfig.h"
#include "game/council/CouncilProposalRegistry.h"

#include <memory>
#include <stdexcept>

namespace ac
{

namespace
{

template <typename T>
const T& RequireRegistry(const std::unique_ptr<T>& pRegistry, const char* fieldName)
{
    if (!pRegistry)
    {
        throw std::runtime_error(std::string("ValidateRequiredTechReferences: GameDataContext.")
                                 + fieldName + " is null");
    }
    return *pRegistry;
}

template <typename TConfig>
void ValidateRequiredTech(const std::vector<TConfig>& rConfigs, const TechRegistry& rTechs,
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
    const TechRegistry& rTechs = RequireRegistry(rData.techRegistry, "techRegistry");

    const BuildingRegistry& rBuildings =
        RequireRegistry(rData.buildingRegistry, "buildingRegistry");
    const ImprovementRegistry& rImprovements =
        RequireRegistry(rData.improvementRegistry, "improvementRegistry");
    const UnitComponentRegistry& rUnitComponents =
        RequireRegistry(rData.unitComponentRegistry, "unitComponentRegistry");
    const UnitSlotRegistry& rUnitSlots =
        RequireRegistry(rData.unitSlotRegistry, "unitSlotRegistry");
    const SocialPolicyRegistry& rSocialPolicies =
        RequireRegistry(rData.socialPolicyRegistry, "socialPolicyRegistry");
    const PopTypeRegistry& rPopTypes =
        RequireRegistry(rData.popTypeRegistry, "popTypeRegistry");
    const CouncilProposalRegistry& rCouncilProposals =
        RequireRegistry(rData.councilProposalRegistry, "councilProposalRegistry");
    const ProbeActionsConfig_t& rProbeActions =
        RequireRegistry(rData.probeActionsConfig, "probeActionsConfig");

    ValidateRequiredTech(rBuildings.GetAll(), rTechs, "Building");
    ValidateRequiredTech(rImprovements.GetAll(), rTechs, "Improvement");
    ValidateRequiredTech(rUnitComponents.GetAll(), rTechs, "Unit component");
    ValidateRequiredTech(rUnitSlots.GetAll(), rTechs, "Unit slot");
    ValidateRequiredTech(rSocialPolicies.GetAll(), rTechs, "Social policy");
    ValidateRequiredTech(rPopTypes.GetAll(), rTechs, "Pop type");
    ValidateRequiredTech(rCouncilProposals.GetAll(), rTechs, "Council proposal");

    for (const ProbeActionConfig_t& rAction : rProbeActions.actions)
    {
        if (!rAction.requiredTech.empty() && !rTechs.Find(rAction.requiredTech))
        {
            throw std::runtime_error(
                std::string("Probe action '") + ProbeActionIdToString(rAction.id)
                + "' has required_tech '" + rAction.requiredTech
                + "' which is not a known tech");
        }
    }
}

} // namespace ac
