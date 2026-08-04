#include "game/EffectReferenceValidator.h"

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
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionRegistry.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/effects/BonusEffect.h"

#include <functional>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

[[noreturn]] void ThrowBadReference_(const std::string& rSourceId, const char* what,
                                     const std::string& rBadId)
{
    throw std::runtime_error("Effect on '" + rSourceId + "' references unknown " + what
                             + " '" + rBadId + "'");
}

} // namespace

void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs,
                              const UnitComponentRegistry* pUnitComponents)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        if (const auto* pGrant = std::get_if<GrantBuildingEffect_t>(&rEffect.effect))
        {
            if (pBuildings && !pBuildings->Find(pGrant->buildingId))
            {
                ThrowBadReference_(rSourceId, "building", pGrant->buildingId);
            }
        }
        else if (const auto* pTech = std::get_if<GrantTechEffect_t>(&rEffect.effect))
        {
            if (pTechs && !pTechs->Find(pTech->techId))
            {
                ThrowBadReference_(rSourceId, "tech", pTech->techId);
            }
        }
        else if (const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect))
        {
            if (pModifier->selector && pModifier->selector->improvement && pImprovements
                && !pImprovements->Find(*pModifier->selector->improvement))
            {
                ThrowBadReference_(rSourceId, "selector improvement",
                                   *pModifier->selector->improvement);
            }
        }

        if (!rEffect.removedByTech.empty() && pTechs && !pTechs->Find(rEffect.removedByTech))
        {
            ThrowBadReference_(rSourceId, "tech", rEffect.removedByTech);
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
                    ThrowBadReference_(rSourceId, "condition feature", rFeatureId);
                }
            };
            std::function<void(const Condition_t&)> checkCondition = [&](const Condition_t& rCond)
            {
                switch (rCond.kind)
                {
                    case ConditionKind_t::IsDefending:
                    case ConditionKind_t::OriginBaseIsTargetBase:
                    case ConditionKind_t::AttackerIsEmbarked:
                        break;
                    case ConditionKind_t::TargetTileHas:
                        checkFeature(rCond.value);
                        break;
                    case ConditionKind_t::AllOf:
                        for (const std::string& rFeatureId : rCond.values)
                        {
                            checkFeature(rFeatureId);
                        }
                        for (const Condition_t& rNested : rCond.conditions)
                        {
                            checkCondition(rNested);
                        }
                        break;
                }
            };
            checkCondition(*rEffect.condition);
        }

        if (rEffect.unitFilter && rEffect.unitFilter->kind == UnitFilterKind_t::HasComponent
            && rEffect.unitFilter->component && pUnitComponents
            && !pUnitComponents->Find(*rEffect.unitFilter->component))
        {
            ThrowBadReference_(rSourceId, "unitFilter component",
                               *rEffect.unitFilter->component);
        }
    }
}

void ValidateEffectReferences(const GameDataContext& rData)
{
    const BuildingRegistry* pBuildings = rData.buildingRegistry.get();
    const ImprovementRegistry* pImprovements = rData.improvementRegistry.get();
    const TechRegistry* pTechs = rData.techRegistry.get();
    const UnitComponentRegistry* pUnitComponents = rData.unitComponentRegistry.get();

    auto validate = [&](const std::vector<EffectConfig_t>& rEffects, const std::string& rSourceId)
    {
        ValidateEffectReferences(rEffects, rSourceId, pBuildings, pImprovements, pTechs,
                                 pUnitComponents);
    };

    if (pBuildings)
    {
        for (const BuildingConfig_t& rConfig : pBuildings->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (pImprovements)
    {
        for (const ImprovementConfig_t& rConfig : pImprovements->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.popTypeRegistry)
    {
        for (const PopTypeConfig_t& rConfig : rData.popTypeRegistry->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.unitComponentRegistry)
    {
        for (const UnitComponentConfig_t& rConfig : rData.unitComponentRegistry->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.socialPolicyRegistry)
    {
        for (const SocialPolicyConfig_t& rConfig : rData.socialPolicyRegistry->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.socialRatingRegistry)
    {
        for (const SocialRatingConfig_t& rConfig : rData.socialRatingRegistry->GetAll())
        {
            for (const auto& [level, rEffects] : rConfig.levelEffects)
            {
                validate(rEffects, rConfig.id + " level " + std::to_string(level));
            }
        }
    }
    if (rData.factionRegistry)
    {
        for (const FactionConfig_t& rConfig : rData.factionRegistry->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.councilProposalRegistry)
    {
        for (const CouncilProposalConfig_t& rConfig : rData.councilProposalRegistry->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.councilRules)
    {
        validate(rData.councilRules->governorEffects, "council_governor");
    }
    validate(rData.tileYieldRules, "tile_yield_rules");
}

} // namespace ac
