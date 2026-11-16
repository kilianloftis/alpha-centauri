#pragma once

namespace ac
{

// Idle-upkeep heal baselines from config/healing.json. Tile, territory, and building
// modifiers stack via HealRate / MaxHealPercent; see docs/thinker/terranx-map.md
// "Unit healing (repair_phase)".
struct HealingConfig_t
{
    // Seed for HealRate when no modifiers apply (binary value = 1 → 10% of max HP).
    int baseHealPercent = 10;
    // Seed for MaxHealPercent (binary field repair_limit → 80% of max HP). Bases raise this
    // through MaxHealPercent on the Base improvement.
    int maxHealPercent = 80;

    // Which UnitTurnEvent_t facts block idle heal when present this turn.
    bool blockHealOnMoveSpend = true;
    bool blockHealOnAttacked = true;
    bool blockHealOnCombat = true;
    bool blockHealOnTookDamage = true;
};

} // namespace ac
