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
#include "game/units/NativeUnitConfig.h"
#include "game/units/NativeUnitRegistry.h"
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
// Id-bearing arms check registries; all others are explicit no-ops.
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
    void operator()(const InterceptEffect_t&) const {}
    void operator()(const ScrambleEffect_t&) const {}
    void operator()(const TransportParamsEffect_t&) const {}
    void operator()(const InteractionOverrideEffect_t&) const {}
};

// The same for TriggeredEffectVariant_t. A separate visitor rather than a shared one: the two
// variants have no alternatives in common, and an exhaustive visitor per family is what makes
// adding an alternative to either break the build here.
struct TriggeredPayloadValidator
{
    const std::string& rSourceId;
    const BuildingRegistry* pBuildings;
    const TechRegistry* pTechs;
    const UnitComponentRegistry* pUnitComponents;

    void operator()(const AddBuildingEffect_t& rAdd) const
    {
        if (pBuildings && !pBuildings->Find(rAdd.buildingId))
        {
            ThrowBadReference(rSourceId, "building", rAdd.buildingId);
        }
    }

    void operator()(const GrantTechEffect_t& rTech) const
    {
        if (!rTech.techId || !pTechs)
        {
            return;
        }
        if (!pTechs->Find(*rTech.techId))
        {
            ThrowBadReference(rSourceId, "tech", *rTech.techId);
        }
    }

    // Validated here rather than at spawn: a granted unit may be assembled hours into a
    // session, and a typo should fail at startup naming the config, not mid-rule.
    void operator()(const GrantUnitEffect_t& rGrant) const
    {
        if (!pUnitComponents)
        {
            return;
        }
        for (const std::string& rId : rGrant.componentIds)
        {
            if (!pUnitComponents->Find(rId))
            {
                ThrowBadReference(rSourceId, "unit component", rId);
            }
        }
    }

    void operator()(const GrantEnergyEffect_t&) const {}
    void operator()(const WorldParameterEffect_t&) const {}
    void operator()(const SetInfiltrationEffect_t&) const {}
    void operator()(const ModifyPopulationEffect_t&) const {}
    void operator()(const GrantXpEffect_t&) const {}
    void operator()(const RestoreHitPointsEffect_t&) const {}
    void operator()(const DestroyFacilityEffect_t&) const {}
    void operator()(const RebelEffect_t&) const {}
    void operator()(const DestroyUnitEffect_t&) const {}
};

void ValidateConditionReferences_(const Condition_t& rCondition,
                                  const std::string& rSourceId,
                                  const ImprovementRegistry* pImprovements,
                                  const UnitComponentRegistry* pUnitComponents,
                                  const NativeUnitRegistry* pNativeUnits,
                                  const BuildingRegistry* pBuildings)
{
    auto checkFeature = [&](const std::string& rFeatureId)
    {
        if (pImprovements && !pImprovements->Find(rFeatureId))
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
                else if constexpr (std::is_same_v<T, HasComponent_t>)
                {
                    if (pUnitComponents && !pUnitComponents->Find(rAlt.component))
                    {
                        ThrowBadReference(rSourceId, "condition component", rAlt.component);
                    }
                }
                else if constexpr (std::is_same_v<T, SubjectDesign_t>)
                {
                    if (pNativeUnits && !pNativeUnits->Find(rAlt.designId))
                    {
                        ThrowBadReference(rSourceId, "condition design", rAlt.designId);
                    }
                }
                else if constexpr (std::is_same_v<T, BaseHasBuilding_t>)
                {
                    if (pBuildings && !pBuildings->Find(rAlt.buildingId))
                    {
                        ThrowBadReference(rSourceId, "condition building", rAlt.buildingId);
                    }
                }
                else if constexpr (std::is_same_v<T, IsDefending_t>
                                   || std::is_same_v<T, OriginBaseIsTargetBase_t>
                                   || std::is_same_v<T, OriginBaseIsHomeBase_t>
                                   || std::is_same_v<T, AttackerIsEmbarked_t>
                                   || std::is_same_v<T, HasAirdroppedThisTurn_t>
                                   || std::is_same_v<T, AttackerDomain_t>
                                   || std::is_same_v<T, DefenderDomain_t>
                                   || std::is_same_v<T, IsHeadquarters_t>
                                   || std::is_same_v<T, SubjectDomain_t>
                                   || std::is_same_v<T, HasFlag_t>
                                   || std::is_same_v<T, IsPrototype_t>
                                   || std::is_same_v<T, IsCombatUnit_t>)
                {
                }
                else
                {
                    static_assert(k_AlwaysFalse<T>, "Unhandled Condition_t alternative");
                }
            },
            rCond.AsVariant());
    };
    checkCondition(rCondition);
}

} // namespace

void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs,
                              const UnitComponentRegistry* pUnitComponents,
                              const SocialRatingRegistry* pSocialRatings,
                              const NativeUnitRegistry* pNativeUnits)
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

        if (rEffect.condition)
        {
            ValidateConditionReferences_(*rEffect.condition, rSourceId, pImprovements,
                                         pUnitComponents, pNativeUnits, pBuildings);
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

void ValidateTriggeredEffectReferences(const std::vector<TriggeredEffectConfig_t>& rEffects,
                                       const std::string& rSourceId,
                                       const BuildingRegistry* pBuildings,
                                       const TechRegistry* pTechs,
                                       const UnitComponentRegistry* pUnitComponents,
                                       const ImprovementRegistry* pImprovements,
                                       const NativeUnitRegistry* pNativeUnits)
{
    for (const TriggeredEffectConfig_t& rEffect : rEffects)
    {
        std::visit(TriggeredPayloadValidator{rSourceId, pBuildings, pTechs, pUnitComponents},
                   rEffect.effect);
        if (rEffect.condition)
        {
            ValidateConditionReferences_(*rEffect.condition, rSourceId, pImprovements,
                                         pUnitComponents, pNativeUnits, pBuildings);
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
    const NativeUnitRegistry& rNativeUnits =
        RequireRegistry(rData.nativeUnitRegistry, "nativeUnitRegistry");

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
                                 &rUnitComponents, &rSocialRatings, &rNativeUnits);
    };
    auto validateTriggered = [&](const std::vector<TriggeredEffectConfig_t>& rEffects,
                                 const std::string& rSourceId)
    {
        ValidateTriggeredEffectReferences(rEffects, rSourceId, &rBuildings, &rTechs,
                                          &rUnitComponents, &rImprovements, &rNativeUnits);
    };

    for (const BuildingConfig_t& rConfig : rBuildings.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
        validateTriggered(rConfig.onCompleteEffects, rConfig.id);
        validateTriggered(rConfig.onUnitProducedEffects, rConfig.id);
    }
    for (const StockpileConfig_t& rConfig : rStockpiles.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const TechConfig_t& rConfig : rTechs.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
        validateTriggered(rConfig.onDiscoverEffects, rConfig.id);
    }
    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
        validateTriggered(rConfig.onVisitEffects, rConfig.id);
    }
    for (const PopTypeConfig_t& rConfig : rPopTypes.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
    }
    for (const UnitComponentConfig_t& rConfig : rUnitComponents.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
        validateTriggered(rConfig.onCompleteEffects, rConfig.id);
        validateTriggered(rConfig.onHoldEffects, rConfig.id);
    }
    for (const NativeUnitConfig_t& rConfig : rNativeUnits.GetAll())
    {
        validate(rConfig.effects, rConfig.id);
        validateTriggered(rConfig.onHoldEffects, rConfig.id);
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
        validateTriggered(rConfig.onPassedEffects, rConfig.id);
    }
    validate(rCouncilRules.governorEffects, "council_governor");
    validateTriggered(rCouncilRules.onElectedEffects, "council_governor");
    for (const ProbeActionConfig_t& rAction : rProbeActions.actions)
    {
        const std::string sourceId =
            std::string("probe_action:") + ProbeActionIdToString(rAction.id);
        validate(rAction.effects, sourceId);
        validateTriggered(rAction.onSuccessEffects, sourceId);
    }
    // tileYieldRules is a value on GameDataContext (always present; effects may be empty).
    validate(rData.tileYieldRules.effects, "tile_yield_rules");
    validate(rData.policeRules, "police_rules");
    const PopCompositionConfig_t& rPopComposition =
        RequireRegistry(rData.popCompositionConfig, "popCompositionConfig");
    validate(rPopComposition.effects, "pop_composition");
    validate(rPopComposition.goldenAgeEffects, "pop_composition.golden_age_effects");
    for (std::size_t tier = 0; tier < rPopComposition.riotTiers.size(); ++tier)
    {
        const std::string sourceId = "pop_composition.riot_tiers[" + std::to_string(tier) + "]";
        validate(rPopComposition.riotTiers[tier].effects, sourceId);
        validateTriggered(rPopComposition.riotTiers[tier].onEnterEffects, sourceId);
    }
    validate(rProductionConfig.effects, "production");
    validateTriggered(rProductionConfig.onUnitProducedEffects, "production");
    validate(RequireRegistry(rData.baseConquestConfig, "baseConquestConfig").effects,
             "base_conquest");
    for (const DifficultyLevel_t& rLevel : rDifficultyConfig.levels)
    {
        validate(rLevel.effects, "difficulty:" + rLevel.id);
    }
}

} // namespace ac
