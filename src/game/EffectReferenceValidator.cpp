#include "game/EffectReferenceValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/production/StockpileRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainFeatureValidation.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/population/PopTypeConfigParser.h"
#include "game/population/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionRegistry.h"
#include "game/production/ProductionConfigParser.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/DifficultyConfig.h"
#include "game/natives/NativeLifeConfig.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/world-events/WorldEventConfig.h"
#include "game/base/BaseConquestConfig.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/natives/NativeUnitConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/probes/ProbeActionConfig.h"
#include "game/effects/EffectConfig.h"

#include <magic_enum.hpp>

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
    const BuildingRegistry& rBuildings;
    const ImprovementRegistry& rImprovements;
    const SocialRatingRegistry& rSocialRatings;

    void operator()(const GrantBuildingEffect_t& rGrant) const
    {
        if (!rBuildings.Find(rGrant.buildingId))
        {
            ThrowBadReference(rSourceId, "building", rGrant.buildingId);
        }
    }

    void operator()(const StatModifierEffect_t& rModifier) const
    {
        if (!rModifier.selector)
        {
            return;
        }
        if (const auto* pHas =
                std::get_if<TileSelectorHasImprovement_t>(&*rModifier.selector))
        {
            if (!rImprovements.Find(pHas->improvement))
            {
                ThrowBadReference(rSourceId, "selector improvement", pHas->improvement);
            }
        }
    }

    void operator()(const InfiltrationEffect_t&) const {}
    void operator()(const RuleFlagEffect_t&) const {}
    void operator()(const SocialEngineeringOverrideEffect_t&) const {}
    void operator()(const DiplomaticModifierEffect_t&) const {}
    // The rating axis must exist: SocialRatingResolver looks it up once the policy's total is
    // non-zero.
    void operator()(const SocialRatingModifierEffect_t& rModifier) const
    {
        if (!rSocialRatings.Find(SocialRatingIdToString(rModifier.rating)))
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

    void operator()(const CoexistenceOverrideEffect_t& rOverride) const
    {
        for (const std::string& rId : {rOverride.firstId, rOverride.secondId})
        {
            if (!rId.empty() && !rImprovements.Find(rId))
            {
                ThrowBadReference(rSourceId, "coexistence occupant", rId);
            }
        }
    }
};

// The same for TriggeredEffectVariant_t, exhaustive so a new alternative breaks the build here.
struct TriggeredPayloadValidator
{
    const std::string& rSourceId;
    const BuildingRegistry& rBuildings;
    const TechRegistry& rTechs;
    const UnitComponentRegistry& rUnitComponents;
    const ImprovementRegistry& rImprovements;

    void operator()(const AddBuildingEffect_t& rAdd) const
    {
        if (!rBuildings.Find(rAdd.buildingId))
        {
            ThrowBadReference(rSourceId, "building", rAdd.buildingId);
        }
    }

    void operator()(const GrantTechEffect_t& rTech) const
    {
        if (rTech.techId && !rTechs.Find(*rTech.techId))
        {
            ThrowBadReference(rSourceId, "tech", *rTech.techId);
        }
    }

    // Checked at load: a unit may be granted hours into a session.
    void operator()(const GrantUnitEffect_t& rGrant) const
    {
        for (const std::string& rId : rGrant.componentIds)
        {
            if (!rUnitComponents.Find(rId))
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
    void operator()(const EarthquakeEffect_t&) const {}
    void operator()(const FungalBloomEffect_t&) const {}
    void operator()(const ExplosionEffect_t&) const {}
    void operator()(const StepRockinessEffect_t&) const {}
    void operator()(const ElevationChangeEffect_t&) const {}

    void operator()(const CommitAtrocityEffect_t&) const {}
    void operator()(const RecordMindControlEffect_t&) const {}
    void operator()(const GrantCleanMineralsEffect_t&) const {}
    void operator()(const AddVirtualMineralsEffect_t&) const {}

    // Only terrain a tile stores itself can be set; depth bands and the two axes derive from
    // elevation.
    void operator()(const SetTerrainFeatureEffect_t& rSet) const
    {
        const ImprovementConfig_t* pConfig = rImprovements.Find(rSet.featureId);
        if (!pConfig || pConfig->placement != OccupantPlacement_t::Terrain)
        {
            ThrowBadReference(rSourceId, "terrain feature", rSet.featureId);
        }
        if (IsDerivedTerrainId(rSet.featureId))
        {
            throw std::runtime_error("'" + rSourceId + "' cannot set terrain '" + rSet.featureId
                                     + "': it is derived from elevation or a terrain axis");
        }
    }
};

// Exhaustive over Condition_t, as EffectPayloadValidator is over effects.
struct ConditionReferenceValidator
{
    const std::string& rSourceId;
    const ImprovementRegistry& rImprovements;
    const UnitComponentRegistry& rUnitComponents;
    const NativeUnitRegistry& rNativeUnits;
    const BuildingRegistry& rBuildings;

    void Check(const Condition_t& rCondition) const
    {
        std::visit(*this, rCondition.AsVariant());
    }

    void operator()(const TargetTileHas_t& rHas) const
    {
        if (!rImprovements.Find(rHas.featureId))
        {
            ThrowBadReference(rSourceId, "condition feature", rHas.featureId);
        }
    }

    void operator()(const AllOf_t& rAll) const
    {
        for (const Condition_t& rNested : rAll.conditions)
        {
            Check(rNested);
        }
    }

    void operator()(const AnyOf_t& rAny) const
    {
        for (const Condition_t& rNested : rAny.conditions)
        {
            Check(rNested);
        }
    }

    void operator()(const HasComponent_t& rHas) const
    {
        if (!rUnitComponents.Find(rHas.component))
        {
            ThrowBadReference(rSourceId, "condition component", rHas.component);
        }
    }

    void operator()(const SubjectDesign_t& rDesign) const
    {
        if (!rNativeUnits.Find(rDesign.designId))
        {
            ThrowBadReference(rSourceId, "condition design", rDesign.designId);
        }
    }

    void operator()(const BaseHasBuilding_t& rHas) const
    {
        CheckBuilding_(rHas.buildingId);
    }

    void operator()(const OriginBaseHasBuilding_t& rHas) const
    {
        CheckBuilding_(rHas.buildingId);
    }

    void operator()(const IsDefending_t&) const {}
    void operator()(const IsInCombat_t&) const {}
    void operator()(const OriginBaseIsHomeBase_t&) const {}
    void operator()(const SubjectIsOnOriginBase_t&) const {}
    void operator()(const SocialRating_t&) const {}
    void operator()(const AttackerIsEmbarked_t&) const {}
    void operator()(const HasAirdroppedThisTurn_t&) const {}
    void operator()(const AttackerDomain_t&) const {}
    void operator()(const DefenderDomain_t&) const {}
    void operator()(const DefenderIsNativeLife_t&) const {}
    void operator()(const IsHeadquarters_t&) const {}
    void operator()(const SubjectIsOwner_t&) const {}
    void operator()(const SubjectIsForeign_t&) const {}
    void operator()(const SubjectDomain_t&) const {}
    void operator()(const HasFlag_t&) const {}
    void operator()(const IsPrototype_t&) const {}
    void operator()(const IsCombatUnit_t&) const {}
    void operator()(const IsNativeLife_t&) const {}
    void operator()(const DesignPlan_t&) const {}
    void operator()(const PartnerPlayerType_t&) const {}
    void operator()(const PartnerIsFormerOwner_t&) const {}
    void operator()(const VictimizedPartner_t&) const {}
    void operator()(const TargetIsBase_t&) const {}
    void operator()(const GiftKind_t&) const {}

private:
    void CheckBuilding_(const std::string& rBuildingId) const
    {
        if (!rBuildings.Find(rBuildingId))
        {
            ThrowBadReference(rSourceId, "condition building", rBuildingId);
        }
    }
};

// Checks one source's effects against the registries LoadGameData always installs.
class EffectSourceChecker_
{
public:
    explicit EffectSourceChecker_(const GameDataContext& rData)
        : m_rBuildings(RequireRegistry(rData.buildingRegistry, "buildingRegistry"))
        , m_rImprovements(RequireRegistry(rData.improvementRegistry, "improvementRegistry"))
        , m_rTechs(RequireRegistry(rData.techRegistry, "techRegistry"))
        , m_rUnitComponents(RequireRegistry(rData.unitComponentRegistry, "unitComponentRegistry"))
        , m_rSocialRatings(RequireRegistry(rData.socialRatingRegistry, "socialRatingRegistry"))
        , m_rNativeUnits(RequireRegistry(rData.nativeUnitRegistry, "nativeUnitRegistry"))
    {
    }

    void Effects(const std::vector<EffectConfig_t>& rEffects, const std::string& rSourceId) const
    {
        const EffectPayloadValidator payloads{rSourceId, m_rBuildings, m_rImprovements,
                                              m_rSocialRatings};
        for (const EffectConfig_t& rEffect : rEffects)
        {
            std::visit(payloads, rEffect.effect);
            CheckRemovedByTech_(rEffect, rSourceId);
            CheckCondition_(rEffect.condition, rSourceId);
            CheckBuildingFilter_(rEffect, rSourceId);
        }
    }

    void Triggered(const std::vector<TriggeredEffectConfig_t>& rEffects,
                   const std::string& rSourceId) const
    {
        const TriggeredPayloadValidator payloads{rSourceId, m_rBuildings, m_rTechs,
                                                 m_rUnitComponents, m_rImprovements};
        for (const TriggeredEffectConfig_t& rEffect : rEffects)
        {
            std::visit(payloads, rEffect.effect);
            CheckCondition_(rEffect.condition, rSourceId);
        }
    }

private:
    void CheckRemovedByTech_(const EffectConfig_t& rEffect, const std::string& rSourceId) const
    {
        if (!rEffect.removedByTech.empty() && !m_rTechs.Find(rEffect.removedByTech))
        {
            ThrowBadReference(rSourceId, "tech", rEffect.removedByTech);
        }
    }

    void CheckCondition_(const std::optional<Condition_t>& rCondition,
                         const std::string& rSourceId) const
    {
        if (!rCondition)
        {
            return;
        }
        const ConditionReferenceValidator conditions{rSourceId, m_rImprovements,
                                                     m_rUnitComponents, m_rNativeUnits,
                                                     m_rBuildings};
        conditions.Check(*rCondition);
    }

    void CheckBuildingFilter_(const EffectConfig_t& rEffect, const std::string& rSourceId) const
    {
        if (!rEffect.buildingFilter)
        {
            return;
        }
        const auto* pId = std::get_if<BuildingFilterId_t>(&*rEffect.buildingFilter);
        if (pId && !m_rBuildings.Find(pId->buildingId))
        {
            ThrowBadReference(rSourceId, "buildingFilter building", pId->buildingId);
        }
    }

    const BuildingRegistry& m_rBuildings;
    const ImprovementRegistry& m_rImprovements;
    const TechRegistry& m_rTechs;
    const UnitComponentRegistry& m_rUnitComponents;
    const SocialRatingRegistry& m_rSocialRatings;
    const NativeUnitRegistry& m_rNativeUnits;
};

void CheckMapSources_(const GameDataContext& rData, const EffectSourceChecker_& rCheck)
{
    for (const BuildingConfig_t& rConfig : rData.buildingRegistry->GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
        rCheck.Effects(rConfig.destroyedEffects, rConfig.id);
        rCheck.Triggered(rConfig.onCompleteEffects, rConfig.id);
        rCheck.Triggered(rConfig.onUnitProducedEffects, rConfig.id);
    }
    const StockpileRegistry& rStockpiles =
        RequireRegistry(rData.stockpileRegistry, "stockpileRegistry");
    for (const StockpileConfig_t& rConfig : rStockpiles.GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
    }
    for (const TechConfig_t& rConfig : rData.techRegistry->GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
        rCheck.Triggered(rConfig.onDiscoverEffects, rConfig.id);
    }
    for (const ImprovementConfig_t& rConfig : rData.improvementRegistry->GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
        rCheck.Triggered(rConfig.onVisitEffects, rConfig.id);
    }
    // A terrain operation is only its effects, so a typo makes it do nothing.
    const TerrainOperationRegistry& rOperations =
        RequireRegistry(rData.terrainOperationRegistry, "terrainOperationRegistry");
    for (const TerrainOperationConfig_t& rConfig : rOperations.GetAll())
    {
        rCheck.Triggered(rConfig.onCompleteEffects, rConfig.id);
    }
}

void CheckUnitAndPopSources_(const GameDataContext& rData, const EffectSourceChecker_& rCheck)
{
    const PopTypeRegistry& rPopTypes = RequireRegistry(rData.popTypeRegistry, "popTypeRegistry");
    for (const PopTypeConfig_t& rConfig : rPopTypes.GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
    }
    for (const UnitComponentConfig_t& rConfig : rData.unitComponentRegistry->GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
        rCheck.Triggered(rConfig.onCompleteEffects, rConfig.id);
        rCheck.Triggered(rConfig.onHoldEffects, rConfig.id);
        rCheck.Triggered(rConfig.onDetonateEffects, rConfig.id);
    }
    for (const NativeUnitConfig_t& rConfig : rData.nativeUnitRegistry->GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
        rCheck.Triggered(rConfig.onHoldEffects, rConfig.id);
    }
}

void CheckPolicySources_(const GameDataContext& rData, const EffectSourceChecker_& rCheck)
{
    const SocialPolicyRegistry& rSocialPolicies =
        RequireRegistry(rData.socialPolicyRegistry, "socialPolicyRegistry");
    for (const SocialPolicyConfig_t& rConfig : rSocialPolicies.GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
    }
    for (const SocialRatingConfig_t& rConfig : rData.socialRatingRegistry->GetAll())
    {
        for (const auto& [level, rEffects] : rConfig.levelEffects)
        {
            rCheck.Effects(rEffects, rConfig.id + " level " + std::to_string(level));
        }
    }
    const FactionRegistry& rFactions = RequireRegistry(rData.factionRegistry, "factionRegistry");
    for (const FactionConfig_t& rConfig : rFactions.GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
    }
    const CouncilProposalRegistry& rCouncilProposals =
        RequireRegistry(rData.councilProposalRegistry, "councilProposalRegistry");
    for (const CouncilProposalConfig_t& rConfig : rCouncilProposals.GetAll())
    {
        rCheck.Effects(rConfig.effects, rConfig.id);
        rCheck.Triggered(rConfig.onPassedEffects, rConfig.id);
    }
    const CouncilRulesConfig_t& rCouncilRules = RequireRegistry(rData.councilRules, "councilRules");
    rCheck.Effects(rCouncilRules.governorEffects, "council_governor");
    rCheck.Triggered(rCouncilRules.onElectedEffects, "council_governor");
    const ProbeActionsConfig_t& rProbeActions =
        RequireRegistry(rData.probeActionsConfig, "probeActionsConfig");
    rCheck.Effects(rProbeActions.effects, "probe_actions");
    for (const ProbeActionConfig_t& rAction : rProbeActions.actions)
    {
        const std::string sourceId =
            "probe_action:" + std::string(magic_enum::enum_name(rAction.id));
        rCheck.Effects(rAction.effects, sourceId);
        rCheck.Triggered(rAction.onPaidEffects, sourceId);
        rCheck.Triggered(rAction.onSuccessEffects, sourceId);
    }
}

void CheckPopCompositionSources_(const GameDataContext& rData, const EffectSourceChecker_& rCheck)
{
    const PopCompositionConfig_t& rPopComposition =
        RequireRegistry(rData.popCompositionConfig, "popCompositionConfig");
    rCheck.Effects(rPopComposition.effects, "pop_composition");
    rCheck.Effects(rPopComposition.goldenAgeEffects, "pop_composition.golden_age_effects");
    for (std::size_t tier = 0; tier < rPopComposition.riotTiers.size(); ++tier)
    {
        const std::string sourceId = "pop_composition.riot_tiers[" + std::to_string(tier) + "]";
        rCheck.Effects(rPopComposition.riotTiers[tier].effects, sourceId);
        rCheck.Triggered(rPopComposition.riotTiers[tier].onEnterEffects, sourceId);
    }
}

void CheckRuleConfigSources_(const GameDataContext& rData, const EffectSourceChecker_& rCheck)
{
    // tileYieldRules is a value on GameDataContext (always present; effects may be empty).
    rCheck.Effects(rData.tileYieldRules.effects, "tile_yield_rules");
    rCheck.Effects(rData.policeRules, "police_rules");
    rCheck.Effects(rData.worldRules, "world_rules");
    CheckPopCompositionSources_(rData, rCheck);
    const ProductionConfig_t& rProductionConfig =
        RequireRegistry(rData.productionConfig, "productionConfig");
    rCheck.Effects(rProductionConfig.effects, "production");
    rCheck.Triggered(rProductionConfig.onUnitProducedEffects, "production");
    const BaseConquestConfig_t& rConquest =
        RequireRegistry(rData.baseConquestConfig, "baseConquestConfig");
    rCheck.Effects(rConquest.effects, "base_conquest");
    const DifficultyConfig_t& rDifficultyConfig =
        RequireRegistry(rData.difficultyConfig, "difficultyConfig");
    for (const DifficultyLevel_t& rLevel : rDifficultyConfig.levels)
    {
        rCheck.Effects(rLevel.effects, "difficulty:" + rLevel.id);
    }
    const NativeLifeConfig_t& rNativeLifeConfig =
        RequireRegistry(rData.nativeLifeConfig, "nativeLifeConfig");
    for (const NativeLifeLevel_t& rLevel : rNativeLifeConfig.levels)
    {
        rCheck.Effects(rLevel.effects, "native_life_level:" + rLevel.id);
    }
    const WorldEventsConfig_t& rWorldEventsConfig =
        RequireRegistry(rData.worldEventsConfig, "worldEventsConfig");
    for (const WorldEventConfig_t& rEvent : rWorldEventsConfig.events)
    {
        const std::string sourceId = "world_event:" + rEvent.id;
        rCheck.Effects(rEvent.effects, sourceId);
        rCheck.Triggered(rEvent.onStartEffects, sourceId);
        rCheck.Triggered(rEvent.onEndEffects, sourceId);
    }
    const EcoDamageConfig_t& rEcoDamageConfig =
        RequireRegistry(rData.ecoDamageConfig, "ecoDamageConfig");
    rCheck.Effects(rEcoDamageConfig.effects, "eco_damage");
    rCheck.Triggered(rEcoDamageConfig.onPopEffects, "eco_damage.fungal_pop");
}

} // namespace

void ValidateEffectReferences(const GameDataContext& rData)
{
    const EffectSourceChecker_ check(rData);
    CheckMapSources_(rData, check);
    CheckUnitAndPopSources_(rData, check);
    CheckPolicySources_(rData, check);
    CheckRuleConfigSources_(rData, check);
}

} // namespace ac
