# Terraforming, fungus and improvements: parity with the original

Make the final outcome of every Former order, fungal bloom and supply-pod terraform cluster
match `terranx.exe` under shipping config: what is refused, what is destroyed, what the player
is told, and when. Internal logic is ours (`planning.mdc`).

Binary behaviour is recorded in [`docs/thinker/terranx-map.md`](../thinker/terranx-map.md):
"Terraforming" (`Terraform[]`, "Starting a project", "Work"), "Supply pods" (Case 7, Case 11),
and the "Eco-damage fungus" / "Fungal payload" notes after "Fauna".

**Kind** says what closing the gap takes:

- **Config**: change shipping values only.
- **Mechanism**: a new key or option on a mechanism we already have, with the C++ that reads it.
- **C++**: nothing in config can express it yet; the engine lacks the rule.

## Settled in this change

- **Plant Fungus.** Fungus `excludes` the Plant Fungus clear set (Farm, SoilEnricher, Mine,
  SolarCollector, Forest, KelpFarm, MiningPlatform, TidalHarness). `PlantFungus` is
  `blocked_by: ["Ocean"]`. Centauri Psi grants `CoexistenceOverride` Allow `["Fungus"]` beside
  its `TerraformBlockOverride` of `Fungus`, so a Centauri Psi placement onto fungus records a tile
  waiver. Fungus arriving destroys the clear set; anything built onto fungus with the licence
  stays until that fungus leaves.
- **Bloom `destroys`.** The Fungus sweep removes the clear set on every bloom. `destroys` adds
  `Road` and `MagTube` for the supply pod and fungal payload, and nothing for eco damage.

## Differences to resolve

### Starting a project

| # | Difference | Original | Ours | Kind |
|---|---|---|---|---|
| 1 | Mag Tube on fungus | Needs Centauri Psi: the fungus gate tests only the road bit, after the road order is upgraded to Mag Tube | Centauri Empathy lifts `Fungus` for `MagTube` | Config |
| 2 | Road on fungus, Centauri Psi held without Centauri Empathy (tech acquired out of order) | Refused: roads check only Centauri Empathy | Allowed: Centauri Psi's override wild-cards the project | Config (one override per project) or Mechanism (a project list on `TerraformBlockOverride`) |
| 3 | Refusal notices | `NOTINFUNGUS` / `NOTYETFUNGUS` name the operation and the missing tech; every other refusal has its own popup | The hotkey silently does nothing, except Raise/Lower, which have notices | C++ |
| 4 | Solar where it would yield no energy | Refused (`LOWSOLAR`) | Allowed | C++ |
| 5 | Thermal Borehole next to a borehole, or next to an own Former with the borehole order | Refused (`TWOBOREHOLES`) | Allowed | C++ |
| 6 | Thermal Borehole next to a lower land tile | Refused (`NOBOREHOLE`) | Allowed | C++ |
| 7 | Aquifer next to a river | Refused (`NOAQUIFER`) | Allowed | C++ |
| 8 | Farm or Forest on Mount Planet | Refused (`NOTONVOLCANO`) | Allowed | Config |
| 9 | Any project on Mount Planet's centre tile | Refused (`NOTONVOLCANO1`) | Allowed: the footprint is one occupant with no centre | Mechanism (a centre occupant on the landmark footprint) |

### Completing a project

| # | Difference | Original | Ours | Kind |
|---|---|---|---|---|
| 10 | Forest built onto fungus (Centauri Psi) | The fungus is removed | The fungus stays, on the waiver | Config |
| 11 | Farm or Soil Enricher built onto a Thermal Borehole | The Borehole stays; Borehole built onto a Farm removes the Farm | Either order removes the other: `excludes` is symmetric | Mechanism (a one-way coexistence pair) |
| 12 | Losing a Forest to any project but Raise Land | Credits `minerals_harvesting_forest` (5) to the home base, else the nearest own base, with `HARVESTFOREST` | Nothing | C++ |

### Fungal blooms

| # | Difference | Original | Ours | Kind |
|---|---|---|---|---|
| 13 | Supply-pod bloom on tiles that already hold fungus | Every tile of the 3×3 at shelf altitude or above is cleared, fungus or not, including Roads and licensed improvements under existing fungus | Only newly fungused tiles lose anything | Mechanism (bloom tile selection and `destroys` reach) |
| 14 | Supply-pod bloom on deep ocean | Skipped | Fungused | Mechanism (bloom tile selection) |
| 15 | Eco-damage and fungal-payload bloom on deep ocean | Fungused and raised to the shelf | Fungused at its depth | Mechanism (a raise-to-shelf option on the bloom, through `ApplyElevationDelta`) |
| 16 | Eco-damage notice | After the bloom: `FUNGUSGROWS2` "Fungus grows near X! *Improvement* destroyed!" when the base is the player's and something was cleared; else `FUNGUSGROWS`, also shown to a player with a unit on the tile | Before the bloom, to the owner only, never naming what was lost | C++ (the bloom reports what it destroyed) |

### Supply-pod terraform cluster

| # | Difference | Original | Ours | Kind |
|---|---|---|---|---|
| 17 | Clearing before placing | Each passing tile loses fungus and the Plant Fungus clear set, then gets the improvement | Fungus tiles, and tiles where the improvement conflicts with what is there, are skipped; a Farm pod keeps a Mine, a Solar pod keeps a Farm | Mechanism (`destroys` on `TerraformCluster`) |
| 18 | Mine pod on flat land | Skipped | Placed | Mechanism (a tile condition per cluster entry) |
| 19 | Neighbour chance | 2/3 for forest, farm and kelp; 1/2 for mine and solar | 2/3 for every entry | Mechanism (a chance per cluster entry) |
| 20 | Sea pod with a deep-ocean centre | The centre is raised to the shelf and gets kelp | The centre is skipped | Mechanism (same option as #15) |

### Ours, not in the original

| # | Difference | Original | Ours | Kind |
|---|---|---|---|---|
| 21 | Confirming a Former order that destroys improvements | No prompt | No prompt; add an opt-in confirmation, off by default | C++ (setting, rules query, prompt); deviation entry "Confirm before a Former order destroys improvements" `[Ours]` |

## Deviations

- **Borehole leaves an orphaned Soil Enricher** `[Original bug]`: Borehole's clear mask removes
  the Farm but not the Soil Enricher, so the Enricher's nutrient bonus returns if the Borehole
  later goes. Ours removes the Enricher with its Farm (`requires`). Entry in
  [`config/deviations/README.md`](../../config/deviations/README.md).
- Already recorded: "Formers terraform base squares" and "Aquatic deep-ocean terraform until
  Adv. Eco Eng".
