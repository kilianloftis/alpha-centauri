#include "game/EffectReferenceValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionRegistry.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "lib/effects/BonusEffect.h"

#include <algorithm>
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

bool IsTerrainFeatureId_(const std::string& rId)
{
    const std::vector<std::string>& terrainIds = AllTerrainFeatureIds();
    return std::find(terrainIds.begin(), terrainIds.end(), rId) != terrainIds.end();
}

} // namespace

void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs)
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

        // Condition feature ids match Tile::HasFeature: either a terrain feature id or an
        // improvement id.
        if (rEffect.condition && pImprovements
            && !IsTerrainFeatureId_(rEffect.condition->value)
            && !pImprovements->Find(rEffect.condition->value))
        {
            ThrowBadReference_(rSourceId, "condition feature", rEffect.condition->value);
        }
    }
}

void ValidateEffectReferences(const GameDataContext& rData)
{
    const BuildingRegistry* pBuildings = rData.buildingRegistry.get();
    const ImprovementRegistry* pImprovements = rData.improvementRegistry.get();
    const TechRegistry* pTechs = rData.techRegistry.get();

    auto validate = [&](const std::vector<EffectConfig_t>& rEffects, const std::string& rSourceId)
    {
        ValidateEffectReferences(rEffects, rSourceId, pBuildings, pImprovements, pTechs);
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
        for (const SocialPolicyConfig& rConfig : rData.socialPolicyRegistry->GetAll())
        {
            validate(rConfig.effects, rConfig.id);
        }
    }
    if (rData.socialRatingRegistry)
    {
        for (const SocialRatingConfig& rConfig : rData.socialRatingRegistry->GetAll())
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
}

} // namespace ac
