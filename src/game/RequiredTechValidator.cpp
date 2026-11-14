#include "game/RequiredTechValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/population/PopTypeConfigParser.h"
#include "game/population/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/research/TechRequirement.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/probes/ProbeActionConfig.h"
#include "game/council/CouncilProposalRegistry.h"

#include <magic_enum.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

void ValidateRequiredTechsField_(const RequiredTechs_t& rRequired, const TechRegistry& rTechs,
                                 const std::string& rWhat, const std::string& rId)
{
    auto checkList = [&](const std::vector<TechRequirement_t>& rList, const char* pListName)
    {
        for (const TechRequirement_t& rEntry : rList)
        {
            if (!rTechs.Find(rEntry.tech))
            {
                throw std::runtime_error(
                    rWhat + " '" + rId + "' required_techs." + pListName + " has unknown tech '"
                    + rEntry.tech + "'");
            }
        }
    };
    checkList(rRequired.any, "any");
    checkList(rRequired.all, "all");
}

template <typename TConfig>
void ValidateRequiredTechs(const std::vector<TConfig>& rConfigs, const TechRegistry& rTechs,
                           const char* what)
{
    for (const TConfig& rConfig : rConfigs)
    {
        ValidateRequiredTechsField_(rConfig.requiredTechs, rTechs, what, rConfig.id);
    }
}

void ValidateProjectTechs_(const std::string& rId, const FormerProject_t& rProject,
                           const TechRegistry& rTechs, const char* what)
{
    ValidateRequiredTechsField_(rProject.requiredTechs, rTechs, what, rId);
}

void ValidateProbeActionTechs_(const ProbeActionsConfig_t& rProbeActions,
                               const TechRegistry& rTechs)
{
    for (const ProbeActionConfig_t& rAction : rProbeActions.actions)
    {
        const std::string id(magic_enum::enum_name(rAction.id));
        ValidateRequiredTechsField_(rAction.requiredTechs, rTechs, "Probe action", id);
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
    const TerrainOperationRegistry& rOperations =
        RequireRegistry(rData.terrainOperationRegistry, "terrainOperationRegistry");
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

    ValidateRequiredTechs(rBuildings.GetAll(), rTechs, "Building");
    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        if (rConfig.project)
        {
            ValidateProjectTechs_(rConfig.id, *rConfig.project, rTechs, "Improvement");
        }
    }
    for (const TerrainOperationConfig_t& rConfig : rOperations.GetAll())
    {
        ValidateProjectTechs_(rConfig.id, rConfig.project, rTechs, "Terrain operation");
    }
    ValidateRequiredTechs(rUnitComponents.GetAll(), rTechs, "Unit component");
    ValidateRequiredTechs(rUnitSlots.GetAll(), rTechs, "Unit slot");
    ValidateRequiredTechs(rSocialPolicies.GetAll(), rTechs, "Social policy");
    ValidateRequiredTechs(rPopTypes.GetAll(), rTechs, "Pop type");
    ValidateRequiredTechs(rCouncilProposals.GetAll(), rTechs, "Council proposal");
    ValidateProbeActionTechs_(rProbeActions, rTechs);
}

} // namespace ac
