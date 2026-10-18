#pragma once

#include "game/base/BaseTypes.h"
#include "game/economy/CommerceRules.h"
#include "game/economy/EconomyRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include <memory>
#include <vector>

namespace ac
{

class WorkerAssignmentManager;
class EconomyManager;
class CommerceCache;
class BuildingManager;
class BaseManager;
class HomeBaseIndex;
class SocialRatingRegistry;
class Tile;
class TileEffectsContext;

// ResourceManager calculates resource production for a base and accumulates per-turn
// stockpiles. It is owned by BaseManager. The worked yield, inefficiency and the econ / labs /
// psych split are EconomyRules' decisions; this class banks what they return.
// Faction CollectIncome / CollectResearch take econ and labs; psych stays here for composition.
class ResourceManager
{
public:
    // rBase is the owning BaseManager (same object for life; ownership transfer rebinds the
    // faction underneath it). rCommerce supplies Treaty/Pact commerce energy (rebound on transfer).
    ResourceManager(const CommerceCache& rCommerce, const BaseManager& rBase);
    ~ResourceManager();

    // Resource production per turn.
    // rBaseEffects is this base's final effect list (BaseEffectsCache::Get).
    int GetNutrientProduction(const BaseEffects_t& rBaseEffects) const;
    int GetMineralProduction(const BaseEffects_t& rBaseEffects) const;
    // Raw energy after Energy effects, before inefficiency. Does not include commerce.
    int GetEnergyProduction(const BaseEffects_t& rBaseEffects) const;
    // Post-split production; includes commerce from the injected CommerceCache when the
    // owning faction has a bound GameState.
    int GetEconProduction(const BaseEffects_t& rBaseEffects) const;
    int GetLabsProduction(const BaseEffects_t& rBaseEffects) const;
    // Local psych% of post-inefficiency energy + Psych StatModifiers (facilities/specialists).
    int GetPsychProduction(const BaseEffects_t& rBaseEffects) const;

    // Current per-turn nutrient bank (gross production from ProduceResources / stockpile
    // credits; drained by BaseGrowth via ConsumeNutrients). Peekable so Production can ask
    // WouldGrowThisTurn.
    int GetNutrientBank() const;

    // ConsumeMinerals is the leftover-bank drain in BaseManager::ApplyProduction
    // (BaseProduction), when production is enabled.
    int ConsumeNutrients();
    int ConsumeMinerals();
    int ConsumeEcon();
    int ConsumeLabs();

    // Psych available this turn. Non-destructive, unlike the Consume* drains above: pop
    // composition reads it on every recalculation, and it is reset in ProduceResources rather
    // than consumed. See docs/game-rules-decisions.md §6.
    int GetPsych() const;

    // Current per-turn mineral bank (filled by ProduceResources; drained by support /
    // BaseProduction when enabled).
    int GetMineralBank() const;
    // Deduct up to `amount` from the mineral bank. `amount` must be >= 0 and <= bank.
    void SpendMinerals(int amount);

    // Credit `amount` into a per-turn resource bank (nutrients / econ / labs / psych).
    // Used by stockpile surplus conversion; `amount` must be >= 0. Minerals and energy throw:
    // minerals are the conversion input, and energy is not a bank — see AddAllocatedEnergy.
    void AddResource(StatId_t stat, int amount);

    // Credit `energy` the way collected energy is credited: inefficiency first, then the
    // faction's econ/labs/psych split. For stockpile conversion, which happens after
    // ProduceResources has already allocated this turn's tile energy. `energy` must be >= 0.
    //
    // Applies the split only — not the flat Econ/Labs/Psych StatModifiers AllocateBaseEnergy
    // adds. Those were already applied this turn; running them again here would pay every
    // facility's flat bonus twice.
    void AddAllocatedEnergy(int energy);

    // Produce nutrients/minerals and allocate energy into econ/labs/psych stockpiles.
    // Called once per turn per base from the ResourceCollection stage. Commerce energy is
    // resolved from the injected CommerceCache.
    void ProduceResources(const BaseEffects_t& rBaseEffects);

    // Partner commerce lines for this base (UI). Empty when unbound / unpaired. Borrowed
    // from the owner's CommerceCache memo; valid until the next commerce-input change.
    const std::vector<CommercePartnerLine_t>& GetCommercePartners() const;

    // Ownership transfer (BaseManager::RebindFaction): commerce is per-faction, so a
    // transferred base must read the new owner's cache.
    void RebindCommerce(const CommerceCache& rCommerce);

private:
    // Re-pointed by RebindCommerce on ownership transfer.
    const CommerceCache* m_pCommerce;
    const BaseManager& m_rBase;
    int m_nutrients = 0;
    int m_minerals = 0;
    int m_econ = 0;
    int m_labs = 0;
    int m_psych = 0;

    int CalculateResource_(StatId_t stat, const TileResources_t& worked, const BaseEffects_t& rBaseEffects) const;
    int CommerceEnergy_() const;
    EnergySplit_t ProducedSplit_(const BaseEffects_t& rBaseEffects) const;

    void ProduceNutrients_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
    void ProduceMinerals_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
    void AllocateEnergy_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
};

} // namespace ac
