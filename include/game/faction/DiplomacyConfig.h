#pragma once

#include "game/effects/EffectConfig.h"
#include "game/faction/DiplomaticStatus.h"

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
    // FactionPair CommerceRate modifiers. Each side's pool carries them tagged with the other
    // side; a resolved rate of 0 means the pair does not trade.
    std::vector<EffectConfig_t> effects;
    // nullopt: lasts until changed. Otherwise the status steps down after this many turns.
    std::optional<int> durationTurns;
};

struct DiplomacyConfig_t
{
    std::array<DiplomaticStatusRules_t, k_DiplomaticStatusCount> statuses{};

    const DiplomaticStatusRules_t& For(DiplomaticStatus_t status) const
    {
        return statuses.at(static_cast<std::size_t>(status));
    }
};

} // namespace ac
