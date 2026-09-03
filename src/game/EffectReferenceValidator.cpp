#include "game/EffectReferenceValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionRegistry.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/DifficultyConfig.h"
#include "game/units/BaseConquestConfig.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/ProbeActionConfig.h"
#include "game/effects/EffectConfig.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace ac
{

namespace
{

[[noreturn]] void ThrowBadReference(const std::string& rSourceId, const char* what,
                                    const std::string& rBadId)
{
    throw std::runtime_error("Effect on '" + rSourceId + "' references unknown " + what
                             + " '" + rBadId + "'");
}

template <typename T>
const T& RequireRegistry(const std::unique_ptr<T>& pRegistry, const char* fieldName)
{
    if (!pRegistry)
    {
        throw std::runtime_error(std::string("ValidateEffectReferences: GameDataContext.")
                                 + fieldName + " is null");
    }
    return *pRegistry;
}

// Exhaustive over EffectVariant_t: a new alternative without an arm fails to compile.
// Id-bearing arms check registries; all others (including GrantUnit) are explicit no-ops.
struct EffectPayloadValidator
{
    const std::string& rSourceId;
    const BuildingRegistry* pBuildings;
    const ImprovementRegistry* pImprovements;
    const TechRegistry* pTechs;
    const SocialRatingRegistry* pSocialRatings;

    void operator()(const GrantBuildingEffect_t& rGrant) const
    {
        if (pBuildings && !pBuildings->Find(rGrant.buildingId))
        {
            ThrowBadReference(rSourceId, "building", rGrant.buildingId);
        }
    }

    void operator()(const GrantTechEffect_t& rTech) const
    {
        if (pTechs && !pTechs->Find(rTech.techId))
        {
            ThrowBadReference(rSourceId, "tech", rTech.techId);
        }
    }

    void operator()(const StatModifierEffect_t& rModifier) const
    {
        if (!rModifier.selector || !pImprovements)
        {
            return;
        }
        if (const auto* pHas =
                std::get_if<TileSelectorHasImprovement_t>(&*rModifier.selector))
        {
            if (!pImprovements->Find(pHas->improvement))
            {
                ThrowBadReference(rSourceId, "selector improvement", pHas->improvement);
            }
        }
    }

    void operator()(const GrantUnitEffect_t&) const {}
    void operator()(const GrantEnergyEffect_t&) const {}
    void operator()(const WorldParameterEffect_t&) const {}
    void operator()(const InfiltrationEffect_t&) const {}
    void operator()(const RuleFlagEffect_t&) const {}
    void operator()(const SocialEngineeringOverrideEffect_t&) const {}
    void operator()(const DiplomaticModifierEffect_t&) const {}
    // The rating axis has to exist in the rating registry: SocialRatingResolver looks the table
    // up whenever the accumulated total is non-zero, which is the first turn after a player
    // adopts the policy that declares this modifier.
    void operator()(const SocialRatingModifierEffect_t& rModifier) const
    {
        if (pSocialRatings && !pSocialRatings->Find(SocialRatingIdToString(rModifier.rating)))
        {
            ThrowBadReference(rSourceId, "social rating axis",
                              SocialRatingIdToString(rModifier.rating));
        }
    }

    void operator()(const ConcealEffect_t&) const {}
    void operator()(const DetectEffect_t&) const {}
    void operator()(const OrbitalAttackEffect_t&) const {}
    void operator()(const InterceptAttemptEffect_t&) const {}
    void operator()(const TransportParamsEffect_t&) const {}
    void operator()(const PermissionEffect_t&) const {}
    void operator()(const ModifyPopulationEffect_t&) const {}
    void operator()(const DestroyFacilityEffect_t&) const {}
    void operator()(const RebelEffect_t&) const {}
};

} // namespace

void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs,
                              const UnitComponentRegistry* pUnitComponents,
                              const SocialRatingRegistry* pSocialRatings)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        std::visit(
            EffectPayloadValidator{rSourceId, pBuildings, pImprovements, pTechs, pSocialRatings},
            rEffect.effect);

        if (!rEffect.removedByTech.empty() && pTechs && !pTechs->Find(rEffect.removedByTech))
        {
            ThrowBadReference(rSourceId, "tech", rEffect.removedByTech);
        }

        // Condition feature ids match Tile::HasFeature, and every one of them - including the
        // intrinsic TerrainFeature_t ids - is an improvement entry. IsDefending /
        // AttackerIsEmbarked have no feature id.
        if (rEffect.condition && pImprovements)
        {
            auto checkFeature = [&](const std::string& rFeatureId)
            {
                if (!pImprovements->Find(rFeatureId))
                {
                    ThrowBadReference(rSourceId, "condition feature", rFeatureId);
                }
            };
            std::function<void(const Condition_t&)> checkCondition = [&](const Condition_t& rCond)
            {
                std::visit(
                    [&](const auto& rAlt)
                    {
                        using T = std::decay_t<decltype(rAlt)>;
                        if constexpr (std::is_same_v<T, TargetTileHas_t>)
                        {
                            checkFeature(rAlt.featureId);
                        }
                        else if constexpr (std::is_same_v<T, AllOf_t>)
                        {
                            for (const Condition_t& rNested : rAlt.conditions)
                            {
                                checkCondition(rNested);
                            }
                        }
                        else if constexpr (std::is_same_v<T, IsDefending_t>
                                           || std::is_same_v<T, OriginBaseIsTargetBase_t>
                                           || std::is_same_v<T, OriginBaseIsHomeBase_t>
                                           || std::is_same_v<T, AttackerIsEmbarked_t>
                                           || std::is_same_v<T, IsHeadquarters_t>)
                        {
                            // Parameterless predicates: no config ids to resolve.
                        }
                        else
                        {
                            // A void visitor has no missing-return diagnostic, so without this
                            // a new alternative would silently skip reference validation.
                            static_assert(k_AlwaysFalse<T>, "Unhandled Condition_t alternative");
                        }
                    },
                    rCond.AsVariant());
            };
            checkCondition(*rEffect.condition);
        }

        if (rEffect.unitFilter && pUnitComponents)
        {
            if (const auto* pHas =
                    std::get_if<UnitFilterHasComponent_t>(&*rEffect.unitFilter))
            {
                if (!pUnitComponents->Find(pHas->component))
                {
                    ThrowBadReference(rSourceId, "unitFilter component", pHas->component);
                }
            }
        }

        if (rEffect.buildingFilter && pBuildings)
        {
            if (const auto* pId = std::get_if<BuildingFilterId_t>(&*rEffect.buildingFilter))
            {
                if (!pBuildings->Find(pId->buildingId))
                {
                    ThrowBadReference(rSourceId, "buildingFilter building", pId->buildingId);
                }
            }
        }
    }
}

void ValidateEffectReferences(const GameDataContext& rData)
{
    // Target registries LoadGameData always installs — unexpected null means every id check
    // for that family would otherwise pass vacuously.
    const BuildingRegistry& rBuildings =
        RequireRegistry(rData.buildingRegistry, "buildingRegistry");
    const StockpileRegistry& rStockpiles =
        RequireRegistry(rData.stockpileRegistry, "stockpileRegistry");
    const ImprovementRegistry& rImprovements =
        RequireRegistry(rData.improvementRegistry, "improvementRegistry");
    const TechRegistry& rTechs = RequireRegistry(rData.techRegistry, "techRegistry");
    const UnitComponentRegistry& rUnitComponents =
        RequireRegistry(rData.unitComponentRegistry, "unitComponentRegistry");

    // Effect-source registries / configs LoadGameData always populates before calling us.
    const PopTypeRegistry& rPopTypes =
        RequireRegistry(rData.popTypeRegistry, "popTypeRegistry");
    const SocialPolicyRegistry& rSocialPolicies =
        RequireRegistry(rData.socialPolicyRegistry, "socialPolicyRegistry");
    const SocialRatingRegistry& rSocialRatings =
        RequireRegistry(rData.socialRatingRegistry, "socialRatingRegistry");
    const FactionRegistry& rFactions =
        RequireRegistry(rData.factionRegistry, "factionRegistry");
    const CouncilProposalRegistry& rCouncilProposals =
        RequireRegistry(rData.councilProposalRegistry, "councilProposalRegistry");
    const CouncilRulesConfig_t& rCouncilRules =
        RequireRegistry(rData.councilRules, "councilRules");
    const ProbeActionsConfig_t& rProbeActions =
        RequireRegistry(rData.probeActionsConfig, "probeActionsConfig");
    const ProductionConfig_t& rProductionConfig =
        RequireRegistry(rData.productionConfig, "productionConfig");
    const DifficultyConfig_t& rDifficultyConfig =
        RequireRegistry(rData.difficultyConfig, "difficultyConfig");

    auto validate = [&](const std::vector<EffectConfig_t>& rEffects, const std::string& rSourceId)
    {
        ValidateEffectReferences(rEffects, rSourceId, &rBuildings, &rImprovements, &rTechs,
                                 &rUnitComponents, &rSocialRatings);
    };

    for (const BuildingConfig_t& rConfig : rBuildings.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const StockpileConfig_t& rConfig : rStockpiles.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const TechConfig_t& rConfig : rTechs.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const PopTypeConfig_t& rConfig : rPopTypes.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const UnitComponentConfig_t& rConfig : rUnitComponents.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const SocialPolicyConfig_t& rConfig : rSocialPolicies.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const SocialRatingConfig_t& rConfig : rSocialRatings.GetAll())
    {
        for (const auto& [level, rEffects] : rConfig.levelEffects)
        {
            validate(rEffects, rConfig.id + " level " + std::to_string(level));
        }
    }
    for (const FactionConfig_t& rConfig : rFactions.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const CouncilProposalConfig_t& rConfig : rCouncilProposals.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    validate(rCouncilRules.governorEffects, "council_governor");
    for (const ProbeActionConfig_t& rAction : rProbeActions.actions)
    {
        validate(rAction.effects,
                 std::string("probe_action:") + ProbeActionIdToString(rAction.id));
    }
    // tileYieldRules is a value on GameDataContext (always present; effects may be empty).
    validate(rData.tileYieldRules.effects, "tile_yield_rules");
    validate(rData.policeRules, "police_rules");
    validate(RequireRegistry(rData.popCompositionConfig, "popCompositionConfig").effects,
             "pop_composition");
    validate(rProductionConfig.effects, "production");
    validate(RequireRegistry(rData.baseConquestConfig, "baseConquestConfig").effects,
             "base_conquest");
    for (const DifficultyLevel_t& rLevel : rDifficultyConfig.levels)
    {
        validate(rLevel.effects, "difficulty:" + rLevel.id);
    }
}

} // namespace ac
