#pragma once

#include <string>
#include <vector>

namespace ac
{

// Where LoadGameData reads each config. Tests point fields at fixtures.
struct GameDataPaths
{
    std::string popTypes = "config/pop_types.json";
    std::string buildings = "config/buildings";
    // Not under buildings/, which loads every *.json there as buildings.
    std::string mapOverlayChannels = "config/map_overlay_channels.json";
    // Population → map base size stage (size1…sizeN) and optional stage-bump buildings.
    std::string baseSpriteSizes = "config/base_sprite_sizes.json";
    std::string stockpiles = "config/stockpiles.json";
    std::string improvements = "config/improvements.json";
    std::string terrain = "config/terrain.json";
    std::string unitComponents = "config/unit_components";
    std::string unitSlots = "config/unit_slot_config.json";
    std::string nativeUnits = "config/native_units.json";
    std::string techs = "config/techs.json";
    std::string socialPolicies = "config/social_policies.json";
    std::string socialRatings = "config/social_rating_effects.json";
    std::string factions = "config/factions";
    std::string popComposition = "config/pop_composition.json";
    std::string popGrowth = "config/pop_growth.json";
    std::string production = "config/production.json";
    std::string commerce = "config/commerce.json";
    std::string diplomacy = "config/diplomacy.json";
    std::string techCost = "config/tech_cost.lua";
    std::string worldGenPresets = "config/worldGen/presets.json";
    std::string worldGenDecoration = "config/worldGen/decoration.json";
    std::string worldGenLandmarks = "config/worldGen/landmarks.json";
    std::string tileYieldRules = "config/tile_yield_rules.json";
    std::string mapRules = "config/map_rules.json";
    std::string interactionGrids = "config/interaction_grids.json";
    std::string moraleLevels = "config/morale_levels.json";
    std::string probeActions = "config/probe_actions.json";
    std::string baseConquest = "config/base_conquest.json";
    std::string combat = "config/combat.json";
    std::string councilProposals = "config/council/proposals.json";
    std::string councilRules = "config/council/rules.json";
    std::string hotkeys = "config/ui/hotkeys.json";
    std::string standingCaptions = "config/ui/standing_captions.json";
    std::string talk = "config/ui/talk.json";
    // Text packs in lookup order: the extracted SMAC text first, then our own.
    std::vector<std::string> textRoots = {"assets/text", "config/text"};
    std::string textLocale = "en";
    std::string difficulty = "config/difficulty.json";
    std::string policeRules = "config/police_rules.json";
    std::string worldRules = "config/world_rules.json";
    std::string atrocities = "config/atrocities.json";
    std::string ecoDamage = "config/eco_damage.json";
    std::string ecoDamageFormula = "config/eco_damage.lua";
    std::string nativeLife = "config/native_life.json";
    std::string nativeLifeFormula = "config/native_life.lua";
    std::string worldEvents = "config/world_events.json";
};

} // namespace ac
