-- Energy cost to upgrade one unit from an old design to a new one.
-- Binary: upgrade_cost (0x4EFD50) × 10; see docs/thinker/terranx-map.md.
-- Ours works in minerals (never rows); stock energy_per_mineral is 1.
-- UpgradeCostScale (Nano Factory) is applied by UpgradeRules after this returns.
--
-- Variables set by the engine before evaluating energy_cost_formula:
--   new_base_cost              : new design GetBaseCost() (minerals)
--   old_weapon_mineral_cost    : old weapon component mineral_cost
--   new_weapon_mineral_cost    : new weapon component mineral_cost
--   old_armor_mineral_cost     : old armour component mineral_cost
--   new_armor_mineral_cost     : new armour component mineral_cost
--   target_unprototyped        : 1 if the target design is still a prototype, else 0

-- Kept for mods; shipping 1 matches SMAC's 10 energy/row at 10 minerals/row.
local energy_per_mineral = 1

function upgrade_energy_cost_formula()
    local minerals = new_base_cost
    if target_unprototyped ~= 0 then
        local half_up = (new_base_cost + 1) // 2
        local t = new_base_cost + half_up
        minerals = t * (t + 1)
    end
    minerals = minerals + math.max(0, new_weapon_mineral_cost - old_weapon_mineral_cost)
    minerals = minerals + math.max(0, new_armor_mineral_cost - old_armor_mineral_cost)
    return minerals * energy_per_mineral
end

return {
    energy_cost_formula = "upgrade_energy_cost_formula()",
}
