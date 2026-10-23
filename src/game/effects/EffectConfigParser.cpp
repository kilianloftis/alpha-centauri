#include "game/effects/EffectConfigParser.h"

#include "game/effects/TriggeredEffectParser.h"
#include "game/movement/MovementConstants.h"
#include "lib/Rational.h"

#include <magic_enum.hpp>
#include <cmath>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{
namespace EffectConfigParser
{

namespace
{

bool IsTileResourceStat_(StatId_t stat)
{
    return stat == StatId_t::Nutrients || stat == StatId_t::Minerals || stat == StatId_t::Energy;
}

void RequireScope_(EffectScope_t scope,
                   std::initializer_list<EffectScope_t> allowed,
                   const std::string& rErrorMessage)
{
    for (const EffectScope_t allowedScope : allowed)
    {
        if (scope == allowedScope)
        {
            return;
        }
    }
    throw std::runtime_error(rErrorMessage);
}

// Legality per amount source, independent of DomainFor(stat): the subject AmountSourceValue
// needs, and the stats and scopes it allows.
void ValidateElevationEnergySource_(const StatModifierEffect_t& rMod,
                                    const EffectConfig_t& rEffect,
                                    const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    // Energy, ThisTile, no radius: the source reads the receiving
    // tile, so an aura would scale off whatever tile it lands on.
    if (stat != StatId_t::Energy)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' ElevationEnergy is only valid on the energy "
            "stat, got '"
            + rStatWire + "'");
    }
    if (scope != EffectScope_t::ThisTile)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' ElevationEnergy requires scope ThisTile");
    }
    if (rEffect.radius != 0)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' ElevationEnergy cannot carry a radius: the "
            "contribution is read from the receiving tile, not the aura's host");
    }
}

void ValidateMineralsConvertedSource_(const StatModifierEffect_t& rMod,
                                      const EffectConfig_t& rEffect,
                                      const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    // Required subject: Stockpile conversion. Allowed: stockpile-output stats +
    // ThisBase + Continuous (source-kind Stockpile checked in ValidateEffectForSource).
    if (!IsStockpileOutputStat(stat))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' MineralsConverted is only valid on a stockpile "
            "output stat (nutrients, energy, econ, labs, psych), got '"
            + rStatWire
            + "'. Minerals are the conversion input, so converting to them is a loop");
    }
    if (scope != EffectScope_t::ThisBase)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' MineralsConverted requires scope ThisBase");
    }
    if (amount <= 0.0 || !std::isfinite(amount))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' MineralsConverted requires amount > 0");
    }
}

void ValidateBaseSizeSource_(const StatModifierEffect_t& rMod,
                             const EffectConfig_t& rEffect,
                             const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    // Required subject: Base. Allowed on Base-domain stats with base/faction scopes.
    if (DomainFor(stat) != ResolveDomain_t::Base)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BaseSize is only valid on a Base-domain stat, got '"
            + rStatWire + "'");
    }
    if (scope != EffectScope_t::ThisBase
        && scope != EffectScope_t::AllOwnerBases
        && scope != EffectScope_t::FactionGlobal)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BaseSize requires scope ThisBase, AllOwnerBases, "
            "or FactionGlobal");
    }
    if (!std::isfinite(amount))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BaseSize requires a finite amount (per-pop scale)");
    }
}

void ValidateBasesOwnedSource_(const StatModifierEffect_t& rMod,
                               const EffectConfig_t& rEffect,
                               const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    // Required subject: Faction. Allowed on Unit-domain stats with ThisUnit
    // (weapon / chassis components that scale with empire size).
    if (DomainFor(stat) != ResolveDomain_t::Unit)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BasesOwned is only valid on a Unit-domain stat, got '"
            + rStatWire + "'");
    }
    if (scope != EffectScope_t::ThisUnit)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BasesOwned requires scope ThisUnit");
    }
    if (!std::isfinite(amount))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BasesOwned requires a finite amount "
            "(per-base scale)");
    }
}

void ValidateIntrinsicXpSource_(const StatModifierEffect_t& rMod,
                                const EffectConfig_t& rEffect,
                                const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    // Required subject: Unit. Allowed on Unit-domain stats with ThisUnit
    // (e.g. Isle of the Deep cargo = 1 × intrinsic XP).
    if (DomainFor(stat) != ResolveDomain_t::Unit)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' IntrinsicXp is only valid on a Unit-domain "
            "stat, got '"
            + rStatWire + "'");
    }
    if (scope != EffectScope_t::ThisUnit)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' IntrinsicXp requires scope ThisUnit");
    }
    if (rMod.op != ModifierOp_t::Add)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' IntrinsicXp requires op Add");
    }
    if (!std::isfinite(amount))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' IntrinsicXp requires a finite amount "
            "(per-XP scale)");
    }
}

void ValidateBuildingUpkeepSource_(const StatModifierEffect_t& rMod,
                                   const EffectConfig_t& rEffect,
                                   const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    // Required subject: Base. Allowed: Continuous MaxClamp on econ (cap energy
    // commerce at facility upkeep). Base-level scopes only.
    if (stat != StatId_t::Econ)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BuildingUpkeep is only valid on the econ "
            "stat, got '"
            + rStatWire + "'");
    }
    if (rMod.op != ModifierOp_t::MaxClamp)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BuildingUpkeep requires op MaxClamp");
    }
    if (scope != EffectScope_t::ThisBase
        && scope != EffectScope_t::AllOwnerBases
        && scope != EffectScope_t::FactionGlobal)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BuildingUpkeep requires scope ThisBase, "
            "AllOwnerBases, or FactionGlobal");
    }
    if (!std::isfinite(amount))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' BuildingUpkeep requires a finite amount "
            "(upkeep scale)");
    }
}

void ValidateLevelsAboveOpponentSource_(const StatModifierEffect_t& rMod,
                                        const EffectConfig_t& rEffect,
                                        const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    if (rMod.op != ModifierOp_t::AddPercent)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' LevelsAboveOpponent requires op AddPercent");
    }
    if (stat != StatId_t::Attack && stat != StatId_t::Defense)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' LevelsAboveOpponent is only valid on attack "
            "or defense, got '"
            + rStatWire + "'");
    }
    if (scope != EffectScope_t::ThisUnit && scope != EffectScope_t::WorldGlobal)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' LevelsAboveOpponent requires scope ThisUnit "
            "or WorldGlobal");
    }
    if (!std::isfinite(amount))
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' LevelsAboveOpponent requires a finite amount "
            "(percent per elevation level)");
    }
}

void ValidateSocialRatingSource_(const StatModifierEffect_t& rMod,
                                 const EffectConfig_t& rEffect,
                                 const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    if (DomainFor(stat) != ResolveDomain_t::Unit)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' SocialRating is only valid on a Unit-domain "
            "stat, got '"
            + rStatWire + "'");
    }
    if (scope != EffectScope_t::ThisUnit && scope != EffectScope_t::FactionUnits)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' SocialRating requires scope ThisUnit or "
            "FactionUnits");
    }
    if (rMod.op != ModifierOp_t::Add)
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' SocialRating requires op Add");
    }
    if (!rMod.amountSource || !rMod.amountSource->amounts.has_value()
        || rMod.amountSource->amounts->empty())
    {
        throw std::runtime_error(
            "StatModifier 'amount_source' SocialRating requires a non-empty 'amounts' "
            "object");
    }
}

void ValidateAmountSourceLegality_(const StatModifierEffect_t& rMod,
                                   const EffectConfig_t& rEffect,
                                   const std::string& rStatWire)
{
    switch (rMod.amountSource->kind)
    {
        case StatModifierEffect_t::AmountSource_t::ElevationEnergy:
            ValidateElevationEnergySource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::MineralsConverted:
            ValidateMineralsConvertedSource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::BaseSize:
            ValidateBaseSizeSource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::BasesOwned:
            ValidateBasesOwnedSource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::IntrinsicXp:
            ValidateIntrinsicXpSource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::BuildingUpkeep:
            ValidateBuildingUpkeepSource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::LevelsAboveOpponent:
            ValidateLevelsAboveOpponentSource_(rMod, rEffect, rStatWire);
            return;
        case StatModifierEffect_t::AmountSource_t::SocialRating:
            ValidateSocialRatingSource_(rMod, rEffect, rStatWire);
            return;
    }
}

void ParseGrantBuilding_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    GrantBuildingEffect_t grantBuilding;
    grantBuilding.buildingId = parameters.value("building_id", "");
    if (grantBuilding.buildingId.empty())
    {
        throw std::runtime_error("GrantBuilding effect missing required 'building_id'");
    }
    rEffect.effect = grantBuilding;
}

void ParseInfiltration_(const nlohmann::json& /*parameters*/, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::WorldGlobal},
        "Infiltration requires scope FactionGlobal or WorldGlobal");
    if (rEffect.scope == EffectScope_t::FactionGlobal && !rEffect.factionFilter)
    {
        throw std::runtime_error(
            "FactionGlobal Infiltration requires a factionFilter "
            "(WorldGlobal without a filter means all other factions)");
    }
    if (rEffect.factionFilter
        && rEffect.factionFilter->kind == FactionFilterKind_t::ActionTarget)
    {
        // Only a probe mission supplies an action target, and a mission fires a triggered
        // list — there is no continuous context that could resolve this filter.
        throw std::runtime_error(
            "factionFilter ActionTarget requires the triggered 'SetInfiltration' effect, not "
            "the continuous 'Infiltration'");
    }
    rEffect.effect = InfiltrationEffect_t{};
}

std::map<int, double> ParseAmountMap_(const nlohmann::json& rAmounts)
{
    if (!rAmounts.is_object() || rAmounts.empty())
    {
        throw std::runtime_error(
            "StatModifier amount_source amounts must be a non-empty object");
    }
    std::map<int, double> amounts;
    for (const auto& [rKey, rValue] : rAmounts.items())
    {
        std::size_t consumed = 0;
        int key = 0;
        try
        {
            key = std::stoi(rKey, &consumed);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error(
                "StatModifier amount_source amounts keys must be integers, got '" + rKey + "'");
        }
        if (consumed != rKey.size())
        {
            throw std::runtime_error(
                "StatModifier amount_source amounts keys must be integers, got '" + rKey + "'");
        }
        amounts[key] = rValue.get<double>();
    }
    return amounts;
}

void ParseStatModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    StatModifierEffect_t statModifier;
    statModifier.stat = ParseStatId(parameters.value("stat", ""));
    statModifier.op = ParseModifierOp(parameters.value("op", "Add"));
    if (statModifier.stat == StatId_t::MoveCost)
    {
        if (statModifier.op != ModifierOp_t::MaxClamp)
        {
            throw std::runtime_error("StatModifier move_cost requires op MaxClamp");
        }
        if (parameters.contains("amount_source"))
        {
            throw std::runtime_error("StatModifier move_cost does not accept amount_source");
        }
        if (!parameters.contains("amount"))
        {
            throw std::runtime_error("StatModifier move_cost requires 'amount'");
        }
        const Rational_t cost = Rational_t::ParseJson(parameters.at("amount"));
        if (cost.numerator < 0 || cost.denominator <= 0)
        {
            throw std::runtime_error("StatModifier move_cost amount must be non-negative");
        }
        try
        {
            statModifier.amount =
                static_cast<double>(cost.ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint));
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::string("StatModifier move_cost amount: ") + e.what());
        }
    }
    else if (parameters.contains("amount_source"))
    {
        // Any op: the source computes the amount and ApplyModifierStack is op-agnostic.
        // The legality checks below are what constrain it.
        statModifier.amountSource = ParseAmountSource(parameters.at("amount_source"));
        if (statModifier.amountSource->amounts)
        {
            if (parameters.contains("amount"))
            {
                throw std::runtime_error(
                    "StatModifier amount_source amounts cannot be combined with 'amount'");
            }
        }
        else
        {
            // amount is the per-source scale when amount_source is set (default 1).
            statModifier.amount = ParseNumber(parameters, "amount", 1.0);
        }
        ValidateAmountSourceLegality_(statModifier, rEffect, parameters.value("stat", ""));
    }
    else
    {
        statModifier.amount = ParseNumber(parameters, "amount", 0.0);
    }
    // Selectors resolve only during tile yield, so a selector on any other stat would never apply.
    if (parameters.contains("selector"))
    {
        if (!IsTileResourceStat_(statModifier.stat))
        {
            throw std::runtime_error(
                "StatModifier 'selector' is only valid on tile resource "
                "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
        }
        // Tile-yield resolution supplies only a tile subject, so a selector never combines with an
        // amount_source.
        if (statModifier.amountSource)
        {
            throw std::runtime_error(
                "StatModifier 'selector' cannot be combined with 'amount_source'");
        }
        statModifier.selector = ParseTileSelector(parameters.at("selector"));
    }
    // MaxClamp / MinClamp on tile resources must carry a selector so they apply per-tile
    // (via ResolveTileYield) rather than clamping the base-wide production total.
    if ((statModifier.op == ModifierOp_t::MaxClamp || statModifier.op == ModifierOp_t::MinClamp)
        && IsTileResourceStat_(statModifier.stat) && !statModifier.selector)
    {
        throw std::runtime_error(
            "StatModifier MaxClamp/MinClamp on tile resource stats requires a 'selector' "
            "(e.g. AnyTile) so the clamp applies per tile, not to base-level totals");
    }
    statModifier.bypassClamp = parameters.value("bypass_clamp", false);
    if (statModifier.bypassClamp && !IsTileResourceStat_(statModifier.stat))
    {
        throw std::runtime_error(
            "StatModifier 'bypass_clamp' is only valid on tile resource "
            "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
    }
    if (statModifier.bypassClamp && statModifier.op != ModifierOp_t::Add)
    {
        throw std::runtime_error("StatModifier 'bypass_clamp' requires op Add");
    }
    if (statModifier.stat == StatId_t::BombardMinHpPercent
        && statModifier.op == ModifierOp_t::AddPercent)
    {
        throw std::runtime_error(
            "StatModifier bombard_min_hp_percent is already a percent of hit points; "
            "AddPercent would be a percent of a percent. Use Add or MinClamp");
    }
    rEffect.effect = statModifier;
}

void ParseRuleFlag_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RuleFlagEffect_t ruleFlag;
    const std::string flagStr = parameters.value("flag", "");
    if (flagStr.empty())
    {
        throw std::runtime_error("RuleFlag effect missing required 'flag'");
    }
    ruleFlag.flag = ParseRuleFlagId(flagStr);
    if (ruleFlag.flag == RuleFlagId_t::Harbors)
    {
        const std::string domainStr = parameters.value("domain", "");
        if (domainStr.empty())
        {
            throw std::runtime_error("RuleFlag 'harbors' requires a 'domain' string");
        }
        ruleFlag.domain = ParseUnitDomain(domainStr);
    }
    else if (parameters.contains("domain"))
    {
        throw std::runtime_error(
            "RuleFlag 'domain' is only valid with flag 'harbors'");
    }
    rEffect.effect = ruleFlag;
}

void ParseInteractionOverride_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (rEffect.scope != EffectScope_t::ThisUnit
        && rEffect.scope != EffectScope_t::FactionUnits)
    {
        throw std::runtime_error(
            "InteractionOverride scope must be ThisUnit or FactionUnits");
    }

    InteractionOverrideEffect_t overrideFx;
    const std::string gridStr = parameters.value("grid", "");
    if (gridStr.empty())
    {
        throw std::runtime_error("InteractionOverride missing required 'grid'");
    }
    if (gridStr == "enter")
    {
        overrideFx.grid = InteractionGridId_t::Enter;
    }
    else if (gridStr == "attack_tile")
    {
        overrideFx.grid = InteractionGridId_t::AttackTile;
    }
    else if (gridStr == "attack_unit")
    {
        overrideFx.grid = InteractionGridId_t::AttackUnit;
    }
    else if (gridStr == "zoc")
    {
        overrideFx.grid = InteractionGridId_t::Zoc;
    }
    else
    {
        throw std::runtime_error("Unknown InteractionOverride grid: '" + gridStr + "'");
    }

    // cell is required. Either polarity is legal; resolve skips overrides that restate the
    // stock cell (non-default only), so first-match order among overrides never matters.
    const std::string cellStr = parameters.value("cell", "");
    if (cellStr.empty())
    {
        throw std::runtime_error("InteractionOverride missing required 'cell'");
    }
    if (cellStr == "allow")
    {
        overrideFx.cell = InteractionCell_t::Allow;
    }
    else if (cellStr == "deny")
    {
        overrideFx.cell = InteractionCell_t::Deny;
    }
    else
    {
        throw std::runtime_error("Unknown InteractionOverride cell: '" + cellStr + "'");
    }

    auto parseDomainOpt = [&](const char* key) -> std::optional<UnitDomain_t> {
        if (!parameters.contains(key))
        {
            return std::nullopt;
        }
        if (!parameters.at(key).is_string())
        {
            throw std::runtime_error(std::string("InteractionOverride '") + key
                                     + "' must be a string");
        }
        return ParseUnitDomain(parameters.at(key).get<std::string>());
    };

    auto parseSurfaceOpt = [&]() -> std::optional<InteractionSurface_t> {
        if (!parameters.contains("surface"))
        {
            return std::nullopt;
        }
        if (!parameters.at("surface").is_string())
        {
            throw std::runtime_error("InteractionOverride 'surface' must be a string");
        }
        const std::string surfaceStr = parameters.at("surface").get<std::string>();
        if (surfaceStr == "land")
        {
            return InteractionSurface_t::Land;
        }
        if (surfaceStr == "water")
        {
            return InteractionSurface_t::Water;
        }
        throw std::runtime_error("Unknown InteractionOverride surface: '" + surfaceStr + "'");
    };

    auto parseFootingOpt = [&]() -> std::optional<InteractionFooting_t> {
        if (!parameters.contains("footing"))
        {
            return std::nullopt;
        }
        if (!parameters.at("footing").is_string())
        {
            throw std::runtime_error("InteractionOverride 'footing' must be a string");
        }
        const std::string footingStr = parameters.at("footing").get<std::string>();
        if (footingStr == "land")
        {
            return InteractionFooting_t::Land;
        }
        if (footingStr == "water")
        {
            return InteractionFooting_t::Water;
        }
        if (footingStr == "embarked")
        {
            return InteractionFooting_t::Embarked;
        }
        throw std::runtime_error("Unknown InteractionOverride footing: '" + footingStr + "'");
    };

    overrideFx.actorDomain = parseDomainOpt("actor_domain");
    overrideFx.surface = parseSurfaceOpt();
    overrideFx.footing = parseFootingOpt();
    overrideFx.targetDomain = parseDomainOpt("target_domain");

    // actor_domain is the row on every grid; only a column that belongs to another grid needs
    // rejecting.
    const bool bSurface = overrideFx.surface.has_value();
    const bool bFooting = overrideFx.footing.has_value();
    const bool bTarget = overrideFx.targetDomain.has_value();
    switch (overrideFx.grid)
    {
    case InteractionGridId_t::Enter:
        if (bFooting || bTarget)
        {
            throw std::runtime_error(
                "InteractionOverride enter only accepts actor_domain / surface axes");
        }
        break;
    case InteractionGridId_t::AttackTile:
        if (bSurface || bTarget)
        {
            throw std::runtime_error(
                "InteractionOverride attack_tile only accepts actor_domain / footing axes");
        }
        break;
    case InteractionGridId_t::AttackUnit:
    case InteractionGridId_t::Zoc:
        if (bSurface || bFooting)
        {
            throw std::runtime_error(
                "InteractionOverride attack_unit / zoc only accept actor_domain / "
                "target_domain axes");
        }
        break;
    }

    rEffect.effect = overrideFx;
}

void ParseSocialEngineeringOverride_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    // TODO: define parsing when social engineering rules are finalized
    SocialEngineeringOverrideEffect_t seOverride;
    seOverride.category = parameters.value("category", "");
    seOverride.choice = parameters.value("choice", "");
    rEffect.effect = seOverride;
}

void ParseDiplomaticModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    // TODO: define parsing when diplomatic modifier rules are finalized
    DiplomaticModifierEffect_t diplomatic;
    diplomatic.targetFactionId = parameters.value("target_faction_id", "");
    diplomatic.value = static_cast<int>(ParseNumber(parameters, "value", 0.0));
    rEffect.effect = diplomatic;
}

void ParseSocialRatingModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    SocialRatingModifierEffect_t ratingMod;
    const std::string ratingStr = parameters.value("rating", "");
    if (ratingStr.empty())
    {
        throw std::runtime_error("SocialRatingModifier effect missing required 'rating'");
    }
    ratingMod.rating = ParseSocialRatingId(ratingStr);
    ratingMod.amount = static_cast<int>(ParseNumber(parameters, "amount", 0.0));
    rEffect.effect = ratingMod;
}

void ParseConceal_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    ConcealEffect_t conceal;
    conceal.channel = parameters.value("channel", "");
    if (conceal.channel.empty())
    {
        throw std::runtime_error("Conceal effect missing required 'channel'");
    }
    rEffect.effect = conceal;
}

void ParseDetect_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    DetectEffect_t detect;
    detect.channel = parameters.value("channel", "");
    if (detect.channel.empty())
    {
        throw std::runtime_error("Detect effect missing required 'channel'");
    }
    rEffect.effect = detect;
}

void ParseOrbitalAttack_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
        "OrbitalAttack requires scope FactionGlobal or AllOwnerBases");
    OrbitalAttackEffect_t orbitalAttack;
    orbitalAttack.chance = static_cast<int>(RequireNumber(parameters, "chance"));
    if (orbitalAttack.chance < 0 || orbitalAttack.chance > 100)
    {
        throw std::runtime_error("OrbitalAttack 'chance' must be in [0, 100]");
    }
    orbitalAttack.cooldownTurns = static_cast<int>(RequireNumber(parameters, "cooldown_turns"));
    if (orbitalAttack.cooldownTurns < 0)
    {
        throw std::runtime_error("OrbitalAttack 'cooldown_turns' must be >= 0");
    }
    orbitalAttack.chanceOfDestructionOnFail =
        static_cast<int>(ParseNumber(parameters, "chance_of_destruction_on_fail", 0.0));
    if (orbitalAttack.chanceOfDestructionOnFail < 0
        || orbitalAttack.chanceOfDestructionOnFail > 100)
    {
        throw std::runtime_error(
            "OrbitalAttack 'chance_of_destruction_on_fail' must be in [0, 100]");
    }
    rEffect.effect = orbitalAttack;
}

void ParseIntercept_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (!rEffect.condition)
    {
        throw std::runtime_error("Intercept requires a condition");
    }
    InterceptEffect_t intercept;
    intercept.chance = static_cast<int>(RequireNumber(parameters, "chance"));
    if (intercept.chance < 0 || intercept.chance > 100)
    {
        throw std::runtime_error("Intercept 'chance' must be in [0, 100]");
    }
    if (parameters.contains("cooldown_turns"))
    {
        intercept.cooldownTurns = static_cast<int>(RequireNumber(parameters, "cooldown_turns"));
        if (intercept.cooldownTurns < 0)
        {
            throw std::runtime_error("Intercept 'cooldown_turns' must be >= 0");
        }
    }
    intercept.chanceOfDestructionOnFail =
        static_cast<int>(ParseNumber(parameters, "chance_of_destruction_on_fail", 0.0));
    if (intercept.chanceOfDestructionOnFail < 0 || intercept.chanceOfDestructionOnFail > 100)
    {
        throw std::runtime_error(
            "Intercept 'chance_of_destruction_on_fail' must be in [0, 100]");
    }
    rEffect.effect = intercept;
}

void ParseScramble_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (!rEffect.condition)
    {
        throw std::runtime_error("Scramble requires a condition");
    }
    ScrambleEffect_t scramble;
    scramble.range = static_cast<int>(RequireNumber(parameters, "range"));
    if (scramble.range <= 0)
    {
        throw std::runtime_error("Scramble 'range' must be > 0");
    }
    rEffect.effect = scramble;
}

std::vector<UnitDomain_t> ParseCarries_(const nlohmann::json& parameters)
{
    std::vector<UnitDomain_t> carries;
    if (!parameters.contains("carries"))
    {
        return carries;
    }
    if (!parameters.at("carries").is_array())
    {
        throw std::runtime_error("TransportParams 'carries' must be an array");
    }
    for (const auto& rDomainJson : parameters.at("carries"))
    {
        if (!rDomainJson.is_string())
        {
            throw std::runtime_error("TransportParams carries entries must be strings");
        }
        const std::string domain = rDomainJson.get<std::string>();
        carries.push_back(ParseUnitDomain(domain));
    }
    return carries;
}

void ParseTransportParams_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::ThisUnit},
        "TransportParams requires scope ThisUnit");
    TransportParamsEffect_t transport;
    transport.carries = ParseCarries_(parameters);
    transport.requiresHarbor = parameters.value("requires_harbor", false);
    if (transport.carries.empty() && !transport.requiresHarbor)
    {
        throw std::runtime_error(
            "TransportParams requires non-empty 'carries' or 'requires_harbor': true");
    }
    rEffect.effect = transport;
}

InteractionCell_t ParseCoexistenceCell_(const nlohmann::json& parameters)
{
    const std::string cellStr = parameters.value("cell", "");
    if (cellStr.empty())
    {
        throw std::runtime_error("CoexistenceOverride missing required 'cell'");
    }
    if (cellStr == "allow")
    {
        return InteractionCell_t::Allow;
    }
    if (cellStr == "deny")
    {
        return InteractionCell_t::Deny;
    }
    throw std::runtime_error("Unknown CoexistenceOverride cell: '" + cellStr + "'");
}

// One or two occupant ids. Two names the exact pair; one wild-cards the other side.
// Zero would wild-card both and switch coexistence off wholesale, so it is rejected.
std::vector<std::string> ParseCoexistenceOccupants_(const nlohmann::json& parameters)
{
    if (!parameters.contains("occupants") || !parameters.at("occupants").is_array())
    {
        throw std::runtime_error("CoexistenceOverride needs an 'occupants' array of one or "
                                 "two occupant ids");
    }
    const nlohmann::json& rOccupants = parameters.at("occupants");
    if (rOccupants.empty() || rOccupants.size() > 2)
    {
        throw std::runtime_error("CoexistenceOverride 'occupants' must name one or two ids, not "
                                 + std::to_string(rOccupants.size()));
    }
    std::vector<std::string> ids;
    for (const nlohmann::json& rEntry : rOccupants)
    {
        if (!rEntry.is_string() || rEntry.get<std::string>().empty())
        {
            throw std::runtime_error("CoexistenceOverride 'occupants' entries must be "
                                     "non-empty occupant ids");
        }
        ids.push_back(rEntry.get<std::string>());
    }
    return ids;
}

void ParseCoexistenceOverride_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (rEffect.scope != EffectScope_t::ThisUnit
        && rEffect.scope != EffectScope_t::FactionUnits)
    {
        throw std::runtime_error(
            "CoexistenceOverride scope must be ThisUnit or FactionUnits");
    }
    CoexistenceOverrideEffect_t overrideFx;
    overrideFx.cell = ParseCoexistenceCell_(parameters);
    const std::vector<std::string> ids = ParseCoexistenceOccupants_(parameters);
    overrideFx.firstId = ids.front();
    if (ids.size() == 2)
    {
        overrideFx.secondId = ids.back();
    }
    rEffect.effect = overrideFx;
}

using ParseEffectFn_ = void (*)(const nlohmann::json& parameters, EffectConfig_t& rEffect);

const std::unordered_map<std::string, ParseEffectFn_>& EffectTypeParsers_()
{
    static const std::unordered_map<std::string, ParseEffectFn_> k_Parsers = {
        {"GrantBuilding", ParseGrantBuilding_},
        {"Infiltration", ParseInfiltration_},
        {"StatModifier", ParseStatModifier_},
        {"RuleFlag", ParseRuleFlag_},
        {"InteractionOverride", ParseInteractionOverride_},
        {"CoexistenceOverride", ParseCoexistenceOverride_},
        {"SocialEngineeringOverride", ParseSocialEngineeringOverride_},
        {"DiplomaticModifier", ParseDiplomaticModifier_},
        {"SocialRatingModifier", ParseSocialRatingModifier_},
        {"Conceal", ParseConceal_},
        {"Detect", ParseDetect_},
        {"OrbitalAttack", ParseOrbitalAttack_},
        {"Intercept", ParseIntercept_},
        {"Scramble", ParseScramble_},
        {"TransportParams", ParseTransportParams_},
    };
    return k_Parsers;
}


// radius and min_radius: a ring of tiles around a ThisTile host.
void ParseReach_(const nlohmann::json& rEffectJson, EffectConfig_t& rEffect)
{
    rEffect.radius = rEffectJson.value("radius", 0);
    if (rEffect.radius < 0)
    {
        throw std::runtime_error("Effect 'radius' must be >= 0");
    }
    if (rEffect.radius != 0 && rEffect.scope != EffectScope_t::ThisTile)
    {
        throw std::runtime_error("Effect 'radius' is only valid with scope ThisTile");
    }
    rEffect.minRadius = rEffectJson.value("min_radius", 0);
    if (rEffect.minRadius < 0)
    {
        throw std::runtime_error("Effect 'min_radius' must be >= 0");
    }
    if (rEffect.minRadius != 0 && rEffect.scope != EffectScope_t::ThisTile)
    {
        throw std::runtime_error("Effect 'min_radius' is only valid with scope ThisTile");
    }
    if (rEffect.minRadius > rEffect.radius)
    {
        throw std::runtime_error("Effect 'min_radius' (" + std::to_string(rEffect.minRadius)
                                 + ") must be <= 'radius' (" + std::to_string(rEffect.radius)
                                 + "); the effect would reach no tile at all");
    }
}
} // namespace

ModifierOp_t ParseModifierOp(const std::string& rOp)
{
    const auto op = magic_enum::enum_cast<ModifierOp_t>(rOp);
    if (!op.has_value())
    {
        throw std::runtime_error("Unknown modifier op: '" + rOp + "'");
    }
    return *op;
}

StatModifierEffect_t::AmountSourceSpec_t ParseAmountSource(const nlohmann::json& rSource)
{
    if (!rSource.is_object())
    {
        throw std::runtime_error("StatModifier amount_source must be an object with 'kind'");
    }
    if (!rSource.contains("kind"))
    {
        throw std::runtime_error("StatModifier amount_source requires 'kind'");
    }
    const std::string kindWire = rSource.at("kind").get<std::string>();
    const auto kind =
        magic_enum::enum_cast<StatModifierEffect_t::AmountSource_t>(kindWire);
    if (!kind.has_value())
    {
        throw std::runtime_error("Unknown amount_source kind '" + kindWire + "'");
    }
    StatModifierEffect_t::AmountSourceSpec_t spec;
    spec.kind = *kind;
    switch (spec.kind)
    {
        case StatModifierEffect_t::AmountSource_t::SocialRating:
            if (!rSource.contains("rating"))
            {
                throw std::runtime_error(
                    "StatModifier amount_source SocialRating requires 'rating'");
            }
            spec.rating = ParseSocialRatingId(rSource.at("rating").get<std::string>());
            if (!rSource.contains("amounts"))
            {
                throw std::runtime_error(
                    "StatModifier amount_source SocialRating requires 'amounts'");
            }
            spec.amounts = ParseAmountMap_(rSource.at("amounts"));
            break;
        default:
            if (rSource.contains("rating"))
            {
                throw std::runtime_error(
                    "StatModifier amount_source '" + kindWire + "' does not take 'rating'");
            }
            if (rSource.contains("amounts"))
            {
                throw std::runtime_error(
                    "StatModifier amount_source '" + kindWire + "' does not take 'amounts'");
            }
            break;
    }
    return spec;
}

EffectScope_t ParseEffectScope(const std::string& rScope)
{
    const auto scope = magic_enum::enum_cast<EffectScope_t>(rScope);
    if (!scope.has_value())
    {
        throw std::runtime_error("Unknown effect scope: '" + rScope + "'");
    }
    return *scope;
}

double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue)
{
    const auto it = parameters.find(key);
    if (it == parameters.end())
    {
        return defaultValue;
    }
    if (it->is_number())
    {
        return it->get<double>();
    }
    if (it->is_string())
    {
        const std::string& rStr = it->get_ref<const std::string&>();
        try
        {
            std::size_t idx = 0;
            const double value = std::stod(rStr, &idx);
            if (idx != rStr.size())
            {
                throw std::runtime_error(
                    "Invalid numeric string for parameter '" + key
                    + "': trailing characters");
            }
            return value;
        }
        catch (const std::invalid_argument&)
        {
            throw std::runtime_error("Invalid numeric string for parameter '" + key + "'");
        }
        catch (const std::out_of_range&)
        {
            throw std::runtime_error(
                "Numeric string out of range for parameter '" + key + "'");
        }
    }
    throw std::runtime_error("Expected a number or numeric string for parameter '" + key + "'");
}

double RequireNumber(const nlohmann::json& parameters, const std::string& key)
{
    if (!parameters.contains(key))
    {
        throw std::runtime_error("Missing required parameter '" + key + "'");
    }
    return ParseNumber(parameters, key, 0.0);
}

namespace
{

// A required non-empty string at rKey; rWhat names the condition in the error.
std::string RequireConditionString_(const nlohmann::json& rJson, const char* pKey,
                                    const std::string& rWhat)
{
    if (!rJson.contains(pKey) || !rJson.at(pKey).is_string()
        || rJson.at(pKey).get<std::string>().empty())
    {
        throw std::runtime_error(rWhat + " condition requires a non-empty '" + pKey + "'");
    }
    return rJson.at(pKey).get<std::string>();
}

// An optional boolean at rKey, bDefault when absent.
bool OptionalConditionBool_(const nlohmann::json& rJson, const char* pKey, bool bDefault,
                            const std::string& rWhat)
{
    if (!rJson.contains(pKey))
    {
        return bDefault;
    }
    if (!rJson.at(pKey).is_boolean())
    {
        throw std::runtime_error(rWhat + " condition '" + pKey + "' must be a boolean");
    }
    return rJson.at(pKey).get<bool>();
}

std::vector<UnitDomain_t> ParseConditionDomains_(const nlohmann::json& rJson,
                                                 const std::string& rWhat)
{
    if (!rJson.contains("domains") || !rJson.at("domains").is_array()
        || rJson.at("domains").empty())
    {
        throw std::runtime_error(rWhat + " condition requires a non-empty 'domains' array");
    }
    std::vector<UnitDomain_t> domains;
    for (const nlohmann::json& rDomain : rJson.at("domains"))
    {
        if (!rDomain.is_string())
        {
            throw std::runtime_error(rWhat + " condition domains must be strings");
        }
        domains.push_back(ParseUnitDomain(rDomain.get<std::string>()));
    }
    return domains;
}

// "values" desugars to TargetTileHas; "conditions" nests any condition. At least one is given.
std::vector<Condition_t> ParseNestedConditions_(const nlohmann::json& rJson,
                                                const std::string& rWhat)
{
    const auto nonEmptyArray = [&rJson](const char* pKey) {
        return rJson.contains(pKey) && rJson.at(pKey).is_array() && !rJson.at(pKey).empty();
    };
    const bool bHasValues = nonEmptyArray("values");
    const bool bHasConditions = nonEmptyArray("conditions");
    if (!bHasValues && !bHasConditions)
    {
        throw std::runtime_error(
            rWhat + " condition requires a non-empty 'values' and/or 'conditions' array");
    }
    std::vector<Condition_t> nested;
    if (bHasValues)
    {
        for (const nlohmann::json& rValue : rJson.at("values"))
        {
            if (!rValue.is_string() || rValue.get<std::string>().empty())
            {
                throw std::runtime_error(rWhat + " condition values must be non-empty strings");
            }
            nested.push_back(TargetTileHas_t{rValue.get<std::string>()});
        }
    }
    if (bHasConditions)
    {
        for (const nlohmann::json& rNested : rJson.at("conditions"))
        {
            nested.push_back(ParseCondition(rNested));
        }
    }
    return nested;
}

Condition_t ParseSocialRatingCondition_(const nlohmann::json& rJson)
{
    const std::string rating = RequireConditionString_(rJson, "rating", "SocialRating");
    const auto op = magic_enum::enum_cast<CompareOp_t>(rJson.value("op", ""));
    if (!op.has_value())
    {
        throw std::runtime_error("SocialRating condition 'op' must be Eq, Le, or Ge");
    }
    if (!rJson.contains("value") || !rJson.at("value").is_number_integer())
    {
        throw std::runtime_error("SocialRating condition requires an integer 'value'");
    }
    return SocialRating_t{ParseSocialRatingId(rating), *op, rJson.at("value").get<int>()};
}

Condition_t ParseTargetTileHasCondition_(const nlohmann::json& rJson)
{
    const std::string featureId = RequireConditionString_(rJson, "value", "TargetTileHas");
    const bool bPresent = OptionalConditionBool_(rJson, "present", true, "TargetTileHas");
    return TargetTileHas_t{featureId, bPresent};
}

using ParseConditionFn_ = Condition_t (*)(const nlohmann::json& rJson);

const std::unordered_map<std::string, ParseConditionFn_>& ConditionParsers_()
{
    static const std::unordered_map<std::string, ParseConditionFn_> k_Parsers = {
        {"IsDefending",
         [](const nlohmann::json& rJson) -> Condition_t {
             return IsDefending_t{OptionalConditionBool_(rJson, "value", true, "IsDefending")};
         }},
        {"IsInCombat", [](const nlohmann::json&) -> Condition_t { return IsInCombat_t{}; }},
        {"OriginBaseIsHomeBase",
         [](const nlohmann::json&) -> Condition_t { return OriginBaseIsHomeBase_t{}; }},
        {"SubjectIsOnOriginBase",
         [](const nlohmann::json&) -> Condition_t { return SubjectIsOnOriginBase_t{}; }},
        {"SocialRating", ParseSocialRatingCondition_},
        {"OriginBaseHasBuilding",
         [](const nlohmann::json& rJson) -> Condition_t {
             const std::string what = "OriginBaseHasBuilding";
             return OriginBaseHasBuilding_t{RequireConditionString_(rJson, "building", what),
                                            OptionalConditionBool_(rJson, "value", true, what)};
         }},
        {"AttackerIsEmbarked",
         [](const nlohmann::json&) -> Condition_t { return AttackerIsEmbarked_t{}; }},
        {"HasAirdroppedThisTurn",
         [](const nlohmann::json&) -> Condition_t { return HasAirdroppedThisTurn_t{}; }},
        {"AttackerDomain",
         [](const nlohmann::json& rJson) -> Condition_t {
             return AttackerDomain_t{ParseConditionDomains_(rJson, "AttackerDomain")};
         }},
        {"DefenderDomain",
         [](const nlohmann::json& rJson) -> Condition_t {
             return DefenderDomain_t{ParseConditionDomains_(rJson, "DefenderDomain")};
         }},
        {"IsHeadquarters", [](const nlohmann::json&) -> Condition_t { return IsHeadquarters_t{}; }},
        {"SubjectDomain",
         [](const nlohmann::json& rJson) -> Condition_t {
             return SubjectDomain_t{
                 ParseUnitDomain(RequireConditionString_(rJson, "domain", "SubjectDomain"))};
         }},
        {"HasComponent",
         [](const nlohmann::json& rJson) -> Condition_t {
             return HasComponent_t{RequireConditionString_(rJson, "component", "HasComponent")};
         }},
        {"SubjectDesign",
         [](const nlohmann::json& rJson) -> Condition_t {
             return SubjectDesign_t{RequireConditionString_(rJson, "design", "SubjectDesign")};
         }},
        {"HasFlag",
         [](const nlohmann::json& rJson) -> Condition_t {
             return HasFlag_t{ParseRuleFlagId(RequireConditionString_(rJson, "flag", "HasFlag"))};
         }},
        {"IsPrototype", [](const nlohmann::json&) -> Condition_t { return IsPrototype_t{}; }},
        {"IsCombatUnit", [](const nlohmann::json&) -> Condition_t { return IsCombatUnit_t{}; }},
        {"IsNativeLife",
         [](const nlohmann::json& rJson) -> Condition_t {
             return IsNativeLife_t{OptionalConditionBool_(rJson, "value", true, "IsNativeLife")};
         }},
        {"BaseHasBuilding",
         [](const nlohmann::json& rJson) -> Condition_t {
             return BaseHasBuilding_t{
                 RequireConditionString_(rJson, "building", "BaseHasBuilding")};
         }},
        {"AllOf",
         [](const nlohmann::json& rJson) -> Condition_t {
             return AllOf_t{ParseNestedConditions_(rJson, "AllOf")};
         }},
        {"AnyOf",
         [](const nlohmann::json& rJson) -> Condition_t {
             return AnyOf_t{ParseNestedConditions_(rJson, "AnyOf")};
         }},
        {"TargetTileHas", ParseTargetTileHasCondition_},
    };
    return k_Parsers;
}

} // namespace

Condition_t ParseCondition(const nlohmann::json& conditionJson)
{
    const std::string kind = RequireConditionString_(conditionJson, "kind", "Every");
    const auto& rParsers = ConditionParsers_();
    const auto it = rParsers.find(kind);
    if (it == rParsers.end())
    {
        throw std::runtime_error("Unknown condition kind: '" + kind + "'");
    }
    return it->second(conditionJson);
}

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson)
{
    const std::string kindStr = selectorJson.value("kind", "BaseTile");
    if (kindStr == "BaseTile")
    {
        return TileSelectorBaseTile_t{};
    }
    if (kindStr == "HasImprovement")
    {
        const std::string improvementId = selectorJson.value("improvement", "");
        if (improvementId.empty())
        {
            throw std::runtime_error("HasImprovement selector requires a non-empty 'improvement' id");
        }
        return TileSelectorHasImprovement_t{improvementId};
    }
    if (kindStr == "AnyTile")
    {
        return TileSelectorAnyTile_t{};
    }

    throw std::runtime_error("Unknown tile selector kind: '" + kindStr + "'");
}

UnitDomain_t ParseUnitDomain(const std::string& rDomain)
{
    if (rDomain == "land") return UnitDomain_t::Land;
    if (rDomain == "sea")  return UnitDomain_t::Sea;
    if (rDomain == "air")  return UnitDomain_t::Air;
    if (rDomain == "orbital") return UnitDomain_t::Orbital;
    throw std::runtime_error(
        "Unknown unit domain '" + rDomain + "' (expected land, sea, air, or orbital)");
}

BuildingFilter_t ParseBuildingFilter(const nlohmann::json& filterJson)
{
    const std::string kindStr = filterJson.value("kind", "");
    if (kindStr == "All")
    {
        return BuildingFilterAll_t{};
    }
    if (kindStr == "BuildingId")
    {
        const std::string buildingId = filterJson.value("building", "");
        if (buildingId.empty())
        {
            throw std::runtime_error(
                "BuildingId buildingFilter requires a non-empty 'building' id");
        }
        return BuildingFilterId_t{buildingId};
    }
    if (kindStr == "Category")
    {
        return BuildingFilterCategory_t{ParseGameCategoryField(filterJson, "category")};
    }

    throw std::runtime_error("Unknown buildingFilter kind: '" + kindStr + "'");
}

FactionFilter_t ParseFactionFilter(const nlohmann::json& filterJson)
{
    FactionFilter_t filter;
    const std::string kindStr = filterJson.value("kind", "");
    if (kindStr == "ActionTarget")
    {
        filter.kind = FactionFilterKind_t::ActionTarget;
    }
    else if (kindStr == "CouncilMembers")
    {
        filter.kind = FactionFilterKind_t::CouncilMembers;
    }
    else if (kindStr == "PlayerType")
    {
        filter.kind = FactionFilterKind_t::PlayerType;
        const std::string typeStr = filterJson.value("type", "");
        const auto playerType =
            magic_enum::enum_cast<PlayerType_t>(typeStr, magic_enum::case_insensitive);
        if (!playerType)
        {
            throw std::runtime_error("PlayerType factionFilter requires 'type' Player or AI, got '"
                                     + typeStr + "'");
        }
        filter.playerType = *playerType;
    }
    else
    {
        throw std::runtime_error("Unknown factionFilter kind: '" + kindStr + "'");
    }
    return filter;
}

bool IsEffectType(const std::string& rTypeName)
{
    return EffectTypeParsers_().count(rTypeName) != 0;
}

EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson)
{
    EffectConfig_t effect;

    const std::string typeStr = effectJson.at("type").get<std::string>();
    if (TriggeredEffectParser::IsTriggeredEffectType(typeStr))
    {
        throw std::runtime_error(
            "'" + typeStr + "' is a one-shot effect and belongs in a trigger-named list "
            "(on_complete_effects / on_enter_effects / on_visit_effects / …), not 'effects'");
    }
    const std::string scopeStr = effectJson.at("scope").get<std::string>();
    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());

    effect.scope = ParseEffectScope(scopeStr);
    ParseReach_(effectJson, effect);
    if (effectJson.contains("condition"))
    {
        effect.condition = ParseCondition(effectJson.at("condition"));
    }
    if (effectJson.contains("buildingFilter"))
    {
        effect.buildingFilter = ParseBuildingFilter(effectJson.at("buildingFilter"));
    }
    if (effectJson.contains("factionFilter"))
    {
        effect.factionFilter = ParseFactionFilter(effectJson.at("factionFilter"));
    }
    effect.removedByTech = effectJson.value("removed_by_tech", "");

    const auto& parsers = EffectTypeParsers_();
    const auto it = parsers.find(typeStr);
    if (it == parsers.end())
    {
        throw std::runtime_error("Unknown effect type: '" + typeStr + "'");
    }
    it->second(parameters, effect);

    return effect;
}

void ValidateEffectForSource(const EffectConfig_t& rEffect, EffectSourceKind_t sourceKind,
                             const std::string& rSourceId)
{
    const EffectScope_t scope = rEffect.scope;

    if ((scope == EffectScope_t::FactionPair) != (sourceKind == EffectSourceKind_t::DiplomaticStatus))
    {
        throw std::runtime_error(
            "Effect on '" + rSourceId
            + "': scope FactionPair is used by, and only by, diplomatic status effects");
    }

    const auto* pStatModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (pStatModifier
        && pStatModifier->HasAmountSource(StatModifierEffect_t::AmountSource_t::MineralsConverted)
        && sourceKind != EffectSourceKind_t::Stockpile)
    {
        throw std::runtime_error(
            "Effect on '" + rSourceId
            + "': amount_source MineralsConverted is only valid on a stockpile config — nothing "
              "else converts minerals, so it would never resolve");
    }

    if (scope == EffectScope_t::ThisPop && sourceKind != EffectSourceKind_t::PopType)
    {
        throw std::runtime_error("Effect on '" + rSourceId
            + "': scope ThisPop is only meaningful on a pop type config");
    }

    // Mood weights are summed by walking the seated pops, never resolved through
    // ResolveBaseStat — a weight emitted anywhere but a pop type would silently never apply.
    if (pStatModifier
        && (pStatModifier->stat == StatId_t::RiotWeight
            || pStatModifier->stat == StatId_t::GoldenAgeWeight)
        && (sourceKind != EffectSourceKind_t::PopType || scope != EffectScope_t::ThisPop))
    {
        throw std::runtime_error(
            "Effect on '" + rSourceId
            + "': riot_weight / golden_age_weight are per-pop and must be ThisPop on a pop type "
              "config");
    }
    if (scope == EffectScope_t::ThisUnit
        && sourceKind != EffectSourceKind_t::UnitComponent
        && sourceKind != EffectSourceKind_t::MoraleLevel
        && sourceKind != EffectSourceKind_t::NativeUnit)
    {
        throw std::runtime_error("Effect on '" + rSourceId
            + "': scope ThisUnit is only meaningful on a unit component, morale level, "
              "or native unit config");
    }
    if (scope == EffectScope_t::ThisTech)
    {
        if (sourceKind != EffectSourceKind_t::Tech)
        {
            throw std::runtime_error("Effect on '" + rSourceId
                + "': scope ThisTech is only meaningful on a tech config");
        }
        if (pStatModifier == nullptr || pStatModifier->stat != StatId_t::TechCost)
        {
            throw std::runtime_error(
                "Effect on '" + rSourceId
                + "': scope ThisTech only accepts StatModifier tech_cost (research cost "
                  "modifiers for this tech as a research target)");
        }
    }
    if (scope == EffectScope_t::ThisBase || scope == EffectScope_t::ProducedAtThisBase)
    {
        // Listed exhaustively (no default:) so adding an EffectSourceKind_t forces a decision
        // here rather than silently inheriting the rejection.
        bool bCanSupplyOriginBase = false;
        switch (sourceKind)
        {
        case EffectSourceKind_t::Building:
        case EffectSourceKind_t::PopType:
        case EffectSourceKind_t::SocialPolicy:
        case EffectSourceKind_t::SocialRating:
        // Stockpile effects are stamped with the converting base at conversion time.
        case EffectSourceKind_t::Stockpile:
            bCanSupplyOriginBase = true;
            break;
        case EffectSourceKind_t::UnitComponent:
        case EffectSourceKind_t::ProbeAction:
            // No origin base to stamp a continuous ThisBase effect
            // with; it belongs in a triggered list.
            bCanSupplyOriginBase = false;
            break;
        case EffectSourceKind_t::PopCompositionBaseLocal:
            // Mood arrays are collected per base, so ThisBase is stamped with the rioting base
            // by the effects pool / base cache. ProducedAtThisBase is not collected here.
            bCanSupplyOriginBase = scope == EffectScope_t::ThisBase;
            break;
        case EffectSourceKind_t::PopComposition:
        case EffectSourceKind_t::Improvement:
        case EffectSourceKind_t::Faction:
        case EffectSourceKind_t::CouncilProposal:
        case EffectSourceKind_t::CouncilRules:
        case EffectSourceKind_t::TileYieldRules:
        case EffectSourceKind_t::Tech:
        case EffectSourceKind_t::Production:
        case EffectSourceKind_t::Difficulty:
        case EffectSourceKind_t::Growth:
        case EffectSourceKind_t::BaseConquest:
        case EffectSourceKind_t::PoliceRules:
        case EffectSourceKind_t::WorldRules:
        case EffectSourceKind_t::EcoDamage:
        case EffectSourceKind_t::NativeLifeLevel:
        case EffectSourceKind_t::WorldEvent:
        case EffectSourceKind_t::DiplomaticStatus:
        case EffectSourceKind_t::MoraleLevel:
        case EffectSourceKind_t::NativeUnit:
            bCanSupplyOriginBase = false;
            break;
        }
        if (!bCanSupplyOriginBase)
        {
            const char* pScopeName =
                scope == EffectScope_t::ThisBase ? "ThisBase" : "ProducedAtThisBase";
            throw std::runtime_error(
                "Effect on '" + rSourceId + "': scope " + pScopeName
                + " requires a source that can supply an origin base "
                  "(Building, PopType, SocialPolicy, SocialRating, or "
                  "PopCompositionBaseLocal). A unit component or probe action that should act "
                  "on the base at production / mission time wants a triggered list instead.");
        }
    }
}

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson)
{
    std::vector<EffectConfig_t> effects;
    if (rContainerJson.contains("effects"))
    {
        const nlohmann::json& rEffectsJson = rContainerJson.at("effects");
        if (!rEffectsJson.is_array())
        {
            throw std::runtime_error("'effects' must be a JSON array");
        }
        for (const auto& rEffectJson : rEffectsJson)
        {
            effects.push_back(ParseEffectConfig(rEffectJson));
        }
    }
    return effects;
}

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson,
                                         EffectSourceKind_t sourceKind,
                                         const std::string& rSourceId)
{
    std::vector<EffectConfig_t> effects = ParseEffects(rContainerJson);
    for (const EffectConfig_t& rEffect : effects)
    {
        ValidateEffectForSource(rEffect, sourceKind, rSourceId);
    }
    return effects;
}

} // namespace EffectConfigParser
} // namespace ac
