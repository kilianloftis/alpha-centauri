# SMAC facilities (base buildings)

Working catalog for populating `config/buildings/`. Source: `alphax.txt` `#FACILITIES`
(cost, maintenance, tech, obsolete, tagline) and `helpx.txt` `#HELPFACn` (player-facing
effect text). Help is not binary confirmation — confirm mechanics in
`docs/thinker/terranx-map.md` before encoding edge cases.

**Count:** 45 buildable facilities + Stockpile Energy (production item, not a
building in our config — lives in `config/stockpiles.json`). Empty slots 42–64
omitted. Secret projects are separate (`config/buildings/projects.json`).

Alphax mineral costs are the raw `#FACILITIES` values (our JSON often uses a
different scale today).

| # | Facility | Cost | Upkeep | Tech | Obsolete | Tagline | Config effects |
|---|---|---:|---:|---|---|---|---|
| 1 | Headquarters | 5 | 0 | — | — | Efficiency | 1 effect(s) |
| 2 | Children's Creche | 5 | 1 | Ethical Calculus | — | Growth/Effic/Morale | 6 effect(s) |
| 3 | Recycling Tanks | 4 | 0 | Biogenetics | Adv. Ecological Engineering | Bonus Resources | 3 effect(s) |
| 4 | Perimeter Defense | 5 | 0 | Doctrine: Loyalty | — | Defense +100% | 3 effect(s) |
| 5 | Tachyon Field | 12 | 2 | Probability Mechanics | — | All Defense +100% | 3 effect(s) |
| 6 | Recreation Commons | 4 | 1 | Social Psych | Sentient Econometrics | Fewer Drones | 1 effect(s) |
| 7 | Energy Bank | 8 | 1 | Industrial Economics | Quantum Machinery | Economy Bonus | empty |
| 8 | Network Node | 8 | 1 | Information Networks | Self-Aware Machines | Labs Bonus | empty |
| 9 | Biology Lab | 6 | 1 | Centauri Empathy | — | Research and PSI | empty |
| 10 | Skunkworks | 6 | 1 | Advanced Subatomic Theory | — | Prototypes Free | 2 effect(s) |
| 11 | Hologram Theatre | 6 | 3 | Planetary Networks | — | Psych and Fewer Drones | empty |
| 12 | Paradise Garden | 12 | 4 | Sentient Econometrics | — | +2 Talents | empty |
| 13 | Tree Farm | 12 | 3 | Environmental Economics | — | Econ/Psych/Forest | empty |
| 14 | Hybrid Forest | 24 | 4 | Planetary Economics | — | Econ/Psych/Forest | empty |
| 15 | Fusion Lab | 12 | 3 | Fusion Power | — | Econ and Labs Bonus | empty |
| 16 | Quantum Lab | 24 | 4 | Quantum Power | — | Econ and Labs Bonus | empty |
| 17 | Research Hospital | 12 | 3 | Gene Splicing | — | Labs and Psych Bonus | 1 effect(s) |
| 18 | Nanohospital | 24 | 4 | Homo Superior | — | Labs and Psych Bonus | 1 effect(s) |
| 19 | Robotic Assembly Plant | 20 | 4 | Industrial Nanorobotics | — | Minerals Bonus | empty |
| 20 | Nanoreplicator | 32 | 6 | Matter Editation | — | Minerals Bonus | empty |
| 21 | Quantum Converter | 20 | 5 | Quantum Machinery | — | Minerals Bonus | empty |
| 22 | Genejack Factory | 10 | 2 | Retroviral Engineering | — | Minerals; More Drones | 1 effect(s) |
| 23 | Punishment Sphere | 10 | 2 | Advanced Military Algorithms | — | No Drones/-50% Tech | 2 effect(s) |
| 24 | Hab Complex | 8 | 2 | Industrial Automation | — | Increase Population Limit | 1 effect(s) |
| 25 | Habitation Dome | 16 | 4 | Super Tensile Solids | — | Increase Population Limit | 1 effect(s) |
| 26 | Pressure Dome | 8 | 0 | Doctrine: Flexibility | — | Submersion/Resources | 4 effect(s) |
| 27 | Command Center | 4 | 1 | Doctrine: Mobility | — | +2 Morale:Land | empty |
| 28 | Naval Yard | 8 | 2 | Doctrine: Initiative | — | +2 Morale:Sea,Sea Def +100% | 2 effect(s) |
| 29 | Aerospace Complex | 8 | 2 | Doctrine: Air Power | — | +2 Morale:Air,Air Def +100% | 2 effect(s) |
| 30 | Bioenhancement Center | 10 | 2 | Neural Grafting | — | +2 Morale:ALL | empty |
| 31 | Centauri Preserve | 10 | 2 | Centauri Meditation | — | Ecology Bonus | empty |
| 32 | Temple of Planet | 20 | 3 | Secrets of Alpha Centauri | — | Ecology Bonus | empty |
| 33 | Psi Gate | 10 | 2 | Matter Transmission | — | Teleport | empty |
| 34 | Covert Ops Center | 10 | 2 | Pre-Sentient Algorithms | — | Stronger Probe Teams | 1 effect(s) |
| 35 | Brood Pit | 8 | 2 | Centauri Genetics | — | Police/Morale | 2 effect(s) |
| 36 | Aquafarm | 8 | 1 | Progenitor Psych | — | Sea Nutrients | empty |
| 37 | Subsea Trunkline | 12 | 4 | Planetary Economics | — | Sea Minerals | empty |
| 38 | Thermocline Transducer | 8 | 0 | Adaptive Economics | — | Sea Energy | empty |
| 39 | Flechette Defense System | 12 | 2 | N-Space Compression | — | Missile Defense | empty |
| 40 | Subspace Generator | 60 | 5 | Singularity Mechanics | — | Alien Victory | empty |
| 41 | Geosynchronous Survey Pod | 16 | 4 | N-Space Compression | — | Increase sight/sensor bonus | empty |
| 65 | Sky Hydroponics Lab | 12 | 0 | Orbital Spaceflight | — | +1 Nutrient ALL BASES | 1 effect(s) |
| 66 | Nessus Mining Station | 12 | 0 | Self-Aware Machines | — | +1 Minerals ALL BASES | 2 effect(s) |
| 67 | Orbital Power Transmitter | 12 | 0 | Advanced Spaceflight | — | +1 Energy ALL BASES | 1 effect(s) |
| 68 | Orbital Defense Pod | 12 | 0 | Self-Aware Machines | — | Missile Defense | 2 effect(s) |
| 69 | Stockpile Energy | 0 | 0 | — | — | Minerals to Energy | stockpile |

---

## Effect status

Each bullet is one SMAC effect. Tags:

- *(none)* — implemented in shipping config and consumed by game code
- — effects system can express it; missing or incomplete in `buildings.json`
- **[partial]** — some of it works; gap named inline
- **[blocked]** — no seam yet (missing type, consumer, or building→combat/production path)


**Counts:** 41 done, 58 not authored, 8 partial, 8 blocked (115 effect bullets).

### Blocked rollup (needs engine work)

| Gap | Facilities |
|---|---|
| Orbital yield halving / size cap | Sky Hydro, Nessus, Orbital Power (partial: flat +1 works) |
| Psi Gate teleport order | Psi Gate |
| Progenitor generator victory | Subspace Generator |
| Native-only `CostMultiplier` | Brood Pit −25% |
| Base vision from buildings | Geosync sight radius 3 |
| Flechette Intercept aura + stack math | Flechette |
| ODP sacrifice deployed pod | Orbital Defense Pod |

## Per-facility effects

### 1. Headquarters

- **Config id:** `Headquarters`
- **Alphax:** cost 5, upkeep 0, always available
- **Effects:**
  - Adds +1 energy at this base. — _StatModifier energy ThisBase_
  - Removes inefficiency at this base. — _via RuleFlag headquarters_
  - Blocks enemy probe mind control of this base. — _via RuleFlag headquarters + probe actions_

### 2. Children's Creche

- **Config id:** `Childrens_Creche`
- **Alphax:** cost 5, upkeep 1, Ethical Calculus
- **Effects:**
  - Adds +2 Growth at this base. — _SocialRatingModifier growth_
  - Adds +2 Efficiency at this base (helpx says +1; binary/Thinker use +2). — _SocialRatingModifier efficiency ThisBase_
  - Cancels negative Morale for units on this base square and gives them +1 Morale instead. — _FactionUnits morale_* conditions_
  - Reduces this base's vulnerability to enemy mind control. — _probe_distance_scale ×0.5_

### 3. Recycling Tanks

- **Config id:** `Recycling_Tanks`
- **Alphax:** cost 4, upkeep 0, Biogenetics; obsolete Adv. Ecological Engineering
- **Effects:**
  - Adds +1 nutrient on the base square.
  - Adds +1 mineral on the base square.
  - Adds +1 energy on the base square.

### 4. Perimeter Defense

- **Config id:** `Perimeter_Defense`
- **Alphax:** cost 5, upkeep 0, Doctrine: Loyalty
- **Effects:**
  - Adds one base defense multiplier against land attack (doubles land defense; with Tachyon, triples). — _ThisTile `defense` slot `fortification` AddPercent 100 + AttackerDomain land, displacing the Base's `Fallback` ×1.25; Citizens' Defense Force grants it to every base_
  - Skips population loss when the last defender dies. — _last_defender_pop_loss MaxClamp 0_
  - Targeted probe sabotage of this facility is high risk. — _high_risk_sabotage; MissionRisk floors to `protected_risk`_

### 5. Tachyon Field

- **Config id:** `Tachyon_Field`
- **Alphax:** cost 12, upkeep 2, Probability Mechanics
- **Effects:**
  - Adds one base defense multiplier against all attack domains (stacks with Perimeter / Naval Yard / Aerospace). — _ThisTile `defense` slot `fortification` AddPercent 100, displacing the Base's `Fallback` ×1.25_
  - Skips population loss when the last defender dies. — _last_defender_pop_loss MaxClamp 0_
  - Targeted probe sabotage of this facility is high risk. — _high_risk_sabotage; MissionRisk floors to `protected_risk`_

### 6. Recreation Commons

- **Config id:** `Recreation_Commons`
- **Alphax:** cost 4, upkeep 1, Social Psych; obsolete Sentient Econometrics
- **Effects:**
  - Removes two drones at this base.

### 7. Energy Bank

- **Config id:** `Energy_Bank`
- **Alphax:** cost 8, upkeep 1, Industrial Economics; obsolete Quantum Machinery
- **Effects:**
  - Increases economy output at this base by 50%. — _econ AddPercent 50 ThisBase_

### 8. Network Node

- **Config id:** `Network_Node`
- **Alphax:** cost 8, upkeep 1, Information Networks; obsolete Self-Aware Machines
- **Effects:**
  - Increases labs output at this base by 50%. — _labs AddPercent 50 ThisBase_
  - **[partial]** Allows this base to study alien artifacts. — _consumer checks Network_Node by id; no building effect needed, but facility itself has empty effects_

### 9. Biology Lab

- **Config id:** `Biology_Lab`
- **Alphax:** cost 6, upkeep 1, Centauri Empathy
- **Effects:**
  - Adds +2 labs per turn at this base. — _labs Add 2 ThisBase_
  - Gives +1 lifecycle to native lifeforms bred at this base. — _on_unit_produced GrantXp + IsNativeLife (Centauri Preserve pattern)_

### 10. Skunkworks

- **Config id:** `Skunkworks`
- **Alphax:** cost 6, upkeep 1, Advanced Subatomic Theory
- **Effects:**
  - Removes the prototype surcharge for new unit designs built at this base. — _prototype_surcharge_scale ×0; also zeros retool_penalty_scale_

### 11. Hologram Theatre

- **Config id:** `Hologram_Theatre`
- **Alphax:** cost 6, upkeep 3, Planetary Networks
- **Effects:**
  - Removes two drones at this base. — _drones Add −2_
  - Increases psych output at this base by 50%. — _psych AddPercent 50_

### 12. Paradise Garden

- **Config id:** `Paradise_Garden`
- **Alphax:** cost 12, upkeep 4, Sentient Econometrics
- **Effects:**
  - Adds two talents at this base. — _talents Add 2_

### 13. Tree Farm

- **Config id:** `Tree_Farm`
- **Alphax:** cost 12, upkeep 3, Environmental Economics
- **Effects:**
  - Increases psych output at this base by 50%.
  - Increases economy output at this base by 50%.
  - Adds nutrient on forest squares worked by this base. — _nutrients + HasImprovement Forest_
  - Cuts at least half of the ecological damage from terraforming at this base. — _eco_terraform_scale ×0.5; fixtures show pattern_

### 14. Hybrid Forest

- **Config id:** `Hybrid_Forest`
- **Alphax:** cost 24, upkeep 4, Planetary Economics
- **Effects:**
  - Adds nutrient on forest squares worked by this base.
  - Adds energy on forest squares worked by this base.
  - Increases psych output at this base by 50%.
  - Increases economy output at this base by 50%.
  - Together with Tree Farm, eliminates ecological damage from terraforming at this base. — _eco_terraform_scale ×0_
  - Stacks with Tree Farm. — _author both; multiplicative stack_

### 15. Fusion Lab

- **Config id:** `Fusion_Lab`
- **Alphax:** cost 12, upkeep 3, Fusion Power
- **Effects:**
  - Increases economy output at this base by 50%.
  - Increases labs output at this base by 50%.

### 16. Quantum Lab

- **Config id:** `Quantum_Lab`
- **Alphax:** cost 24, upkeep 4, Quantum Power
- **Effects:**
  - Increases economy output at this base by 50%.
  - Increases labs output at this base by 50%.
  - Stacks with Fusion Lab.

### 17. Research Hospital

- **Config id:** `Research_Hospital`
- **Alphax:** cost 12, upkeep 3, Gene Splicing
- **Effects:**
  - Increases psych output at this base by 25%.
  - Increases labs output at this base by 50%.
  - Removes one drone at this base.
  - Reduces population loss from genetic warfare and disease outbreaks. — _plague_damage_scale −50%_

### 18. Nanohospital

- **Config id:** `Nanohospital`
- **Alphax:** cost 24, upkeep 4, Homo Superior
- **Effects:**
  - Increases psych output at this base by 25%.
  - Increases labs output at this base by 50%.
  - Removes one drone at this base.
  - Reduces population loss from genetic warfare and disease outbreaks. — _plague_damage_scale −50%_
  - Stacks with Research Hospital.

### 19. Robotic Assembly Plant

- **Config id:** `Robotic_Assembly_Plant`
- **Alphax:** cost 20, upkeep 4, Industrial Nanorobotics
- **Effects:**
  - Increases minerals output at this base by 50%. — _minerals AddPercent 50_
  - Stacks with other mineral-bonus facilities of this type.

### 20. Nanoreplicator

- **Config id:** `Nanoreplicator`
- **Alphax:** cost 32, upkeep 6, Matter Editation
- **Effects:**
  - Increases minerals output at this base by 50%.
  - Stacks with other mineral-bonus facilities of this type.

### 21. Quantum Converter

- **Config id:** `Quantum_Converter`
- **Alphax:** cost 20, upkeep 5, Quantum Machinery
- **Effects:**
  - Increases minerals output at this base by 50%.
  - Stacks with other mineral-bonus facilities of this type.

### 22. Genejack Factory

- **Config id:** `Genejack_Factory`
- **Alphax:** cost 10, upkeep 2, Retroviral Engineering
- **Effects:**
  - Increases minerals output at this base by 50%.
  - Adds one drone at this base.
  - Increases this base's vulnerability to enemy mind control. — _probe_distance_scale ×2_
  - Stacks with other mineral-bonus facilities of this type.

### 23. Punishment Sphere

- **Config id:** `Punishment_Sphere`
- **Alphax:** cost 10, upkeep 2, Advanced Military Algorithms
- **Effects:**
  - Eliminates drones at this base. — _drones MaxClamp 0 or large negative — confirm composition math_
  - Eliminates talents at this base. — _talents MaxClamp 0_
  - Decreases this base's vulnerability to enemy mind control. — _probe_distance_scale ×0.5 on base + home units_
  - Halves labs output at this base. — _labs MultiplyGeometric 0.5_

### 24. Hab Complex

- **Config id:** `Hab_Complex`
- **Alphax:** cost 8, upkeep 2, Industrial Automation
- **Effects:**
  - Raises the population cap so the base can grow beyond size 7. — _max_base_size Add 7_

### 25. Habitation Dome

- **Config id:** `Habitation_Dome`
- **Alphax:** cost 16, upkeep 4, Super Tensile Solids
- **Effects:**
  - Raises the population cap so the base can grow beyond size 14. — _max_base_size Add 113_
  - Requires Hab Complex already present at this base. — _required_buildings.any Hab_Complex_; `terranx-map.md` § Facility-on-facility

### 26. Pressure Dome

- **Config id:** `Pressure_Dome`
- **Alphax:** cost 8, upkeep 0, Doctrine: Flexibility
- **Effects:**
  - Lets the base occupy and survive on water. — _may_occupy_water_
  - Counts as Recycling Tanks (+1 nutrient, mineral, and energy on the base square). — _flat ThisBase +1s authored_

### 27. Command Center

- **Config id:** `Command_Center`
- **Alphax:** cost 4, upkeep 1, Doctrine: Mobility
- **Effects:**
  - Fully repairs damaged land units at this base in one idle upkeep. — _HealRate MinClamp 100 FactionUnits (SubjectIsOnOriginBase + land + !BaseIsRioting)_; Base tile heal_rate +10 is canceled by riot_tiers Add −10; terranx-map.md § Unit healing
  - Land units built here receive +2 morale. — _on_unit_produced GrantXp +2 land non-native_

### 28. Naval Yard

- **Config id:** `Naval_Yard`
- **Alphax:** cost 8, upkeep 2, Doctrine: Initiative
- **Effects:**
  - Fully repairs damaged sea units at this base in one idle upkeep. — _HealRate MinClamp 100 FactionUnits (SubjectIsOnOriginBase + sea + !BaseIsRioting)_
  - Sea units built here receive +2 morale. — _on_unit_produced GrantXp + SubjectDomain sea_
  - Adds +100% defense against sea attack at this base. — _ThisTile `defense` slot `fortification` AddPercent 100 + AttackerDomain sea; Maritime Control Center grants it to every base_

### 29. Aerospace Complex

- **Config id:** `Aerospace_Complex`
- **Alphax:** cost 8, upkeep 2, Doctrine: Air Power
- **Effects:**
  - Fully repairs damaged air units at this base in one idle upkeep. — _HealRate MinClamp 100 FactionUnits (SubjectIsOnOriginBase + air + !BaseIsRioting)_
  - Air units built here receive +2 morale. — _on_unit_produced GrantXp +2 air_
  - Adds +100% defense against air attack at this base. — _ThisTile `defense` slot `fortification` AddPercent 100 + AttackerDomain air, orbital (missiles are air in SMAC); Cloudbase Academy grants it to every base_
  - Required before this base can produce orbital facilities (and Geosync); Cloudbase Academy or Space Elevator SP also satisfy the gate. — _those buildings' required_buildings.any_; `terranx-map.md` § Facility-on-facility

### 30. Bioenhancement Center

- **Config id:** `Bioenhancement_Center`
- **Alphax:** cost 10, upkeep 2, Neural Grafting
- **Effects:**
  - All military units built here receive +2 morale. — _on_unit_produced GrantXp + IsCombatUnit_
  - Gives +1 lifecycle to native lifeforms bred at this base. — _GrantXp + IsNativeLife_

### 31. Centauri Preserve

- **Config id:** `Centauri_Preserve`
- **Alphax:** cost 10, upkeep 2, Centauri Meditation
- **Effects:**
  - Reduces ecological damage from industry at this base. — _eco_damage_reduction Add; fixtures_
  - Gives +1 lifecycle to native lifeforms bred at this base. — _on_unit_produced GrantXp_

### 32. Temple of Planet

- **Config id:** `Temple_Of_Planet`
- **Alphax:** cost 20, upkeep 3, Secrets of Alpha Centauri
- **Effects:**
  - Reduces ecological damage from industry at this base. — _eco_damage_reduction Add (stacks with Preserve)_
  - Gives +1 lifecycle to native lifeforms bred at this base.
  - Stacks with Centauri Preserve.

### 33. Psi Gate

- **Config id:** `Psi_Gate`
- **Alphax:** cost 10, upkeep 2, Matter Transmission
- **Effects:**
  - **[blocked]** Lets units teleport between bases that both have a Psi Gate. — _TeleportUnit exists as triggered effect only; no Psi Gate order / both-bases gate_

### 34. Covert Ops Center

- **Config id:** `Covert_Ops_Center`
- **Alphax:** cost 10, upkeep 2, Pre-Sentient Algorithms
- **Effects:**
  - Probe teams built here receive +2 morale. — _on_unit_produced GrantXp + probe condition_
  - **[partial]** Adds +2 Probe rating at this base. — _probe_defense +2 authored; SMAC help says PROBE social rating — different seam, same intent for success rolls_

### 35. Brood Pit

- **Config id:** `Brood_Pit`
- **Alphax:** cost 8, upkeep 2, Centauri Genetics
- **Effects:**
  - Native units built here receive +1 lifecycle. — _on_unit_produced GrantXp_
  - **[blocked]** Reduces the mineral cost of alien/native units by 25%. — _CostMultiplier resolves on base effects only — cannot filter IsNativeLife for the queued design_
  - **[partial]** Cancels negative lifecycle effects for units on this base square. — _native morale_extra on origin base authored; not full lifecycle SE cancel_
  - Adds +2 Police rating at this base. — _SocialRatingModifier police +2_

### 36. Aquafarm

- **Config id:** `Aquafarm`
- **Alphax:** cost 8, upkeep 1, Progenitor Psych
- **Effects:**
  - Adds +1 nutrient on Kelp Farms worked by this base. — _HasImprovement KelpFarm_

### 37. Subsea Trunkline

- **Config id:** `Subsea_Trunkline`
- **Alphax:** cost 12, upkeep 4, Planetary Economics
- **Effects:**
  - Adds +1 mineral on Mining Platforms worked by this base. — _HasImprovement MiningPlatform_

### 38. Thermocline Transducer

- **Config id:** `Thermocline_Transducer`
- **Alphax:** cost 8, upkeep 0, Adaptive Economics
- **Effects:**
  - Adds +1 energy on Tidal Harnesses worked by this base. — _HasImprovement TidalHarness_

### 39. Flechette Defense System

- **Config id:** `Flechette_Defense_System`
- **Alphax:** cost 12, upkeep 2, N-Space Compression
- **Effects:**
  - Gives +50% defense against conventional missiles to units within two squares. — _a building ThisTile radius aura from the base tile, as Perimeter's at radius 0_
  - **[blocked]** If you have no Orbital Defense Pods, has a 50% chance to shoot down Planet Busters, Tectonic Missiles, and Fungal Missiles targeting within two squares. — _needs building Intercept aura + 'no ODP' condition; ODP Intercept is FactionGlobal only_
  - **[blocked]** Multiple Flechettes covering the same target stack (two → 25% missile success, and so on). — _Intercept tries sources sequentially; not independent SMAC stack math_

### 40. Subspace Generator

- **Config id:** `Subspace_Generator`
- **Alphax:** cost 60, upkeep 5, Singularity Mechanics
- **Effects:**
  - **[blocked]** Counts toward Progenitor Resonance Communicator victory when built in a size-10+ base. — _no progenitor victory / generator counter_
  - **[blocked]** Six such generators for one Progenitor faction win the game. — _same_

### 41. Geosynchronous Survey Pod

- **Config id:** `Geosynchronous_Survey_Pod`
- **Alphax:** cost 16, upkeep 4, N-Space Compression
- **Effects:**
  - **[blocked]** Extends this base's sighting radius to three squares. — _FactionVisibleMap uses fixed k_BaseVisionRadius=2; building vision not folded in_
  - Acts as a Sensor Array (+25% defensive bonus). — _the Sensor improvement's radius-2 `defense` effect as a building ThisTile aura_
  - **[partial]** Can be destroyed by Orbital Defense Pods or probe teams. — _generic ASAT/probe sabotage can destroy buildings; not Geosync-specific rules_
  - May only be produced at a base with Aerospace Complex, or with Cloudbase Academy / Space Elevator owned by the faction. — _required_buildings.any_

### 65. Sky Hydroponics Lab

- **Config id:** `Sky_Hydroponics_Lab`
- **Alphax:** cost 12, upkeep 0, Orbital Spaceflight; orbital
- **Effects:**
  - **[partial]** Adds +1 nutrient to every base you own (halved without Aerospace Complex; capped at base size). — _AllOwnerBases +1 authored; Aerospace halving and size cap not implemented_
  - May only be produced at a base with Aerospace Complex, or with Cloudbase Academy / Space Elevator owned by the faction. — _required_buildings.any_

### 66. Nessus Mining Station

- **Config id:** `Nessus_Mining_Station`
- **Alphax:** cost 12, upkeep 0, Self-Aware Machines; orbital
- **Effects:**
  - **[partial]** Adds +1 mineral to every base you own (halved without Aerospace Complex; capped at base size). — _+1 + eco_mineral_offset −1 authored; halving/cap missing_
  - May only be produced at a base with Aerospace Complex, or with Cloudbase Academy / Space Elevator owned by the faction. — _required_buildings.any_

### 67. Orbital Power Transmitter

- **Config id:** `Orbital_Power_Transmitter`
- **Alphax:** cost 12, upkeep 0, Advanced Spaceflight; orbital
- **Effects:**
  - **[partial]** Adds +1 energy to every base you own (halved without Aerospace Complex; capped at base size). — _+1 authored; halving/cap missing_
  - May only be produced at a base with Aerospace Complex, or with Cloudbase Academy / Space Elevator owned by the faction. — _required_buildings.any_

### 68. Orbital Defense Pod

- **Config id:** `Orbital_Defense_Pod`
- **Alphax:** cost 12, upkeep 0, Self-Aware Machines; orbital
- **Effects:**
  - **[partial]** Each undeployed pod has a 50% chance to stop a Planet Buster, Fungal, or Tectonic strike on any of your bases. — _Intercept + OrbitalAttack authored; may need DesignPlan vs domain-only match for true PB types_
  - A pod may deploy only once per turn. — _BuildingDeployLedger cooldown_turns 1_
  - **[blocked]** When every pod has already deployed, a Planet Buster can still be stopped by sacrificing a deployed pod. — _no sacrifice/re-arm deployed pod rule_
  - May only be produced at a base with Aerospace Complex, or with Cloudbase Academy / Space Elevator owned by the faction. — _required_buildings.any_

### 69. Stockpile Energy

- **Config id:** `Stockpile_Energy`
- **Alphax:** cost 0, upkeep 0, always available; stockpile not a building
- **Effects:**
  - Converts all minerals produced at the base into energy reserves at 2 minerals → 1 energy. — _stockpiles.json MineralsConverted → econ 0.5_
