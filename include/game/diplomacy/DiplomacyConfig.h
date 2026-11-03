#pragma once

#include "game/effects/EffectConfig.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/diplomacy/GiftSize.h"
#include "lib/Rational.h"

#include <array>
#include <optional>
#include <vector>

namespace ac
{

// How a faction that honors a defensive obligation enters the conflict.
enum class DefensiveObligationMode_t
{
    // Joins on the victim's side (JoinVendetta): the aggressor's own partners are not obliged.
    JoinAsDefender,
    // Makes its own declaration (DeclareVendetta), which obliges the aggressor's partners.
    SeparateDeclaration,
};

// What one diplomatic status permits between the two factions that hold it. Every field but
// effects is required in config/diplomacy.json.
struct DiplomaticStatusRules_t
{
    bool bEnterTerritory = false;
    bool bShareTiles = false;
    bool bRepairAtBases = false;
    bool bMayAttack = false;
    // Whether a player may break this status by attacking; otherwise it ends only in talks.
    bool bBreakByAttack = false;
    bool bDefensiveObligation = false;
    // FactionPair modifiers a rule resolves toward the partner (CommerceRate, where 0 means the
    // pair does not trade; ProbeActorCost). Each side's pool carries them tagged with the other
    // side.
    std::vector<EffectConfig_t> effects;
    // nullopt: lasts until changed. Otherwise the status steps down after this many turns.
    std::optional<int> durationTurns;
};

// value_of_base's coefficients (docs/thinker/terranx-map.md "Base value"). Costs are minerals,
// divided by mineralsPerRow to SMAC's rows once per term.
struct BaseValueConfig_t
{
    int scanRadius = 0;
    int creditsOffset = 0;
    int distanceOffset = 0;
    int minPopulation = 0;
    int minimum = 0;
    Rational_t ungarrisonedFactor;
    int counterpartyAbsentMultiplier = 0;
    int loneBaseMultiplier = 0;
    int outnumberedRatio = 0;
    Rational_t outnumberedFactor;
    int mineralsPerRow = 0;
    int projectRowsMultiplier = 0;
    int unitRowsMultiplier = 0;
};

// One rung of a gift's size ladder: size, when the value reaches the recipient's credits x
// recipientCreditsFactor, the giver's credits x giverCreditsFactor, the turn x turnFactor, and
// minimum. Each product truncates.
struct GiftSizeStepConfig_t
{
    GiftSize_t size = GiftSize_t::Small;
    Rational_t recipientCreditsFactor;
    Rational_t giverCreditsFactor;
    Rational_t turnFactor;
    int minimum = 0;
};

// Rungs tested in order; fallback when none is reached.
struct GiftSizesConfig_t
{
    std::vector<GiftSizeStepConfig_t> steps;
    GiftSize_t fallback = GiftSize_t::Small;
};

// The credit gift's tiered friction: value / creditsPerUnit units; the first firstBlockUnits
// divide by firstBlockDivisor, each further blockUnits by the next divisor up. The sum's ceiling
// over unitsPerPoint, times factor, is the friction drop.
struct CreditFrictionConfig_t
{
    int creditsPerUnit = 0;
    int firstBlockUnits = 0;
    int firstBlockDivisor = 0;
    int blockUnits = 0;
    int unitsPerPoint = 0;
    Rational_t factor;
};

struct CreditGiftConfig_t
{
    GiftSizesConfig_t sizes;
    CreditFrictionConfig_t friction;
};

// tech_val's simple path after the weighted sum (terranx-map.md "Gifts").
struct TechValueConfig_t
{
    // Each weight counts priorityFactor x priority + 1 times.
    int priorityFactor = 0;
    // Scales a tech that matches no priority, rounding up.
    Rational_t unmatchedFactor;
    // A faction with fewer bases matches any growth tech.
    int growthBaseCount = 0;
    // Each owner past the first lowers an unowned tech's value by this.
    int extraOwnerPenalty = 0;
    int minimum = 0;
};

struct TechGiftConfig_t
{
    GiftSizesConfig_t sizes;
    Rational_t frictionFactor;
    TechValueConfig_t value;
};

struct BaseGiftConfig_t
{
    GiftSizesConfig_t sizes;
    Rational_t frictionFactor;
};

struct GiftConfig_t
{
    CreditGiftConfig_t credits;
    TechGiftConfig_t technology;
    BaseGiftConfig_t base;
};

// A change to one faction's blemishes: add delta, then raise the total to floor.
struct BlemishChange_t
{
    int delta = 0;
    int floor = 0;
};

struct DiplomacyConfig_t
{
    std::array<DiplomaticStatusRules_t, k_DiplomaticStatusCount> statuses{};
    DefensiveObligationMode_t defensiveObligationMode{};
    int frictionMax = 0;
    int blemishesMax = 0;
    int breakAgreementBlemishes = 0;
    // Exclusive bound of the friction draw a probe victim that keeps its agreement adds toward
    // the actor.
    int probeKeepFrictionDraw = 0;
    // Friction an AI victim outside Vendetta gains toward a faction that captures its artifacts.
    int artifactCaptureFriction = 0;
    // Friction a faction gains toward an attacker that fights its Vendetta enemy in its territory
    // while the two hold an agreement. Negative: the attacker earns goodwill.
    int alliedTerritoryBattleFriction = 0;
    // The same battle raises the attacker's gift count toward the owner to at least this.
    int alliedTerritoryBattleGiftsFloor = 0;
    // Friction an AI faction gains toward a human whose Former's raise or lower land dried the
    // land around its base.
    int terraformRainfallFriction = 0;
    // The grievant scan stops after the first moisture change within this distance of one of the
    // terraformer's bases.
    int terraformRainfallStopDistance = 0;
    BaseValueConfig_t baseValue;
    GiftConfig_t gifts;

    const DiplomaticStatusRules_t& For(DiplomaticStatus_t status) const
    {
        return statuses.at(static_cast<std::size_t>(status));
    }
};

} // namespace ac
