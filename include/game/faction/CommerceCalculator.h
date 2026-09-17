#pragma once

#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseTypes.h"

#include <unordered_map>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class GameState;
struct CommerceConfig_t;
class LuaRuntime;

// One Friendship/Pact partner that contributes commerce to a specific base this turn.
struct CommercePartnerLine_t
{
    const Faction* pPartner = nullptr;
    DiplomaticStatus_t status = DiplomaticStatus_t::None;
    int ourEnergy = 0;
    int theirEnergy = 0;
};

// Pure commerce income math: pairs Friendship/Pact bases by pre-commerce energy and returns
// per-base commerce energy for the owning faction. Does not mutate ResourceManager or treasury.
// Pair income is one Lua formula from commerce.json; CommerceRate then CommerceEnergyBonus
// are applied in C++ after.
// TODO: zero commerce when sanctions are in effect against either faction.
class CommerceCalculator
{
public:
    // rConfig and rLua outlive every faction (GameDataContext owns both).
    CommerceCalculator(const CommerceConfig_t& rConfig, LuaRuntime& rLua);

    // Every base of rOwner that earns commerce, with its per-partner breakdown. One
    // planet-wide pass: each faction's bases are ranked once and reused across every pair,
    // so this is the entry point to prefer over calling ComputeForBase in a loop.
    // Bases with no eligible pair are absent rather than present-and-empty.
    std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> ComputeAllLines(
        const Faction& rOwner, const GameState& rGameState) const;

    // Per-base commerce energy for rOwner this turn (missing keys mean 0).
    std::unordered_map<BaseId_t, int> ComputeForFaction(const Faction& rOwner,
                                                        const GameState& rGameState) const;

    // Partner breakdown for one base (UI). Empty when unpaired or on no eligible treaty, and
    // for a base not (yet) in its faction's list — mid CreateBaseFromSnapshot / RebindFaction
    // a base is momentarily ownerless, which is simply no commerce, not an error.
    // Runs a whole ComputeAllLines pass; per-base callers should go through CommerceManager,
    // which memoizes it.
    std::vector<CommercePartnerLine_t> ComputeForBase(const BaseManager& rBase,
                                                      const GameState& rGameState) const;

private:
    const CommerceConfig_t* m_pConfig;
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime* m_pLua;
};

} // namespace ac
