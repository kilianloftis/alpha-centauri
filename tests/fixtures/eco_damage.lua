-- Ecological damage formula (SMACX, per base).
--
-- Variables set by the engine before evaluating damage_formula:
--   terraform_raw        : sum of eco_damage_contribution over the base radius, plus
--                          eco_damage_worked_contribution on tiles this base's pops work
--   terraform_scale      : resolved eco_terraform_scale (Tree Farm 0.5, Hybrid Forest 0)
--   minerals             : the base's mineral production
--   mineral_offset       : resolved eco_mineral_offset (negative for orbital minerals)
--   clean_minerals       : resolved eco_clean_minerals (the baseline cap)
--   fungal_blooms        : the faction's fungal blooms so far
--   clean_mineral_grants : the faction's clean-mineral grants from eco facilities
--   virtual_minerals     : atrocity and one-shot eco damage, already weighted
--   damage_reduction     : resolved eco_damage_reduction (Goodfacs)
--   techs                : techs this faction has discovered
--   eco_scale            : resolved ecological_damage (difficulty x Planet rating x native
--                          life x perihelion)
--
-- virtual_minerals arrives pre-weighted. Do not multiply by 5 here: that factor is
-- Major.eco_virtual_minerals in config/atrocities.json, and every AddVirtualMinerals entry
-- likewise authors its own amount.
--
-- Returns the percentage chance of a fungal pop in the base radius.

function eco_damage_formula()
    local terraform = (terraform_raw / 8) * terraform_scale

    local cap = clean_minerals + fungal_blooms + clean_mineral_grants

    local clean1 = 0
    if terraform > 0 then clean1 = math.min(cap, terraform) end
    local clean2 = cap - clean1

    local mineral_term =
        (minerals + mineral_offset - clean2 + virtual_minerals) / (1 + damage_reduction)
    local factor = math.max(0, math.floor((terraform - clean1) + mineral_term))

    return math.floor(factor * techs * eco_scale / 300)
end

return { damage_formula = "eco_damage_formula()" }
