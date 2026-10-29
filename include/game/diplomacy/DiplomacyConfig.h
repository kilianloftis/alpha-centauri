#pragma once

#include "game/effects/EffectConfig.h"
#include "game/diplomacy/DiplomaticStatus.h"

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
    bool bDefensiveObligation = false;
    // FactionPair modifiers a rule resolves toward the partner (CommerceRate, where 0 means the
    // pair does not trade; ProbeActorCost). Each side's pool carries them tagged with the other
    // side.
    std::vector<EffectConfig_t> effects;
    // nullopt: lasts until changed. Otherwise the status steps down after this many turns.
    std::optional<int> durationTurns;
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

    const DiplomaticStatusRules_t& For(DiplomaticStatus_t status) const
    {
        return statuses.at(static_cast<std::size_t>(status));
    }
};

} // namespace ac
