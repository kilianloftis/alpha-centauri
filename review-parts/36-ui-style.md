## UI — style

**Files:** `src/ui/style/UiStyle.cpp`, `include/ui/style/UiStyle.h`

**Assessment:** This slice is the config-backed theme extraction that prior review 4.5 deferred (“shared UI theme”). Load fails loudly on missing files/keys, commits into the global only after a full parse, and `Get` throws if used too early — good failure posture. The dominant weakness is structural: a process-global mega-struct plus ~40 hand-rolled section parsers that must be edited for every new UI surface, with duplicated type pairs and world-domain values mixed into presentation config.

### [H] Stop growing a process-global god-object style registry
`include/ui/style/UiStyle.h:574`–`621` and `src/ui/style/UiStyle.cpp:15`–`16`, `690`–`749` — `UiStyle` is both a ~40-member typed bag and a file-scope singleton (`g_style` / `g_loaded`) accessed via `Style()` (`include/ui/style/UiStyle.h:623`). Every new panel requires a new nested struct, a `UiStyle` member, a `Parse*Style_` clone, and another `root.at(...)` line in `Load`. That violates open/closed growth and bypasses the project’s owned definition-data pattern (`GameDataContext`): UI code cannot take a `const UiStyle&` at construction, and tests cannot inject an alternate theme without mutating process state. Split into per-feature style types (or section parsers) loaded into an owned object and passed down from startup/factory; keep `Style()` only as a temporary bridge if needed.

### [M] Collapse duplicate identical style type pairs
`include/ui/style/UiStyle.h:294`–`318` (`GrowthDisplayStyle` / `ProductionDisplayStyle`) and `363`–`389` (`PopTypeSelectorPopupStyle` / `ProductionSelectorPopupStyle`), with matching parse twins at `src/ui/style/UiStyle.cpp:374`–`402` and `453`–`483` — the struct layouts, JSON keys, and current `config/ui/style.json` values are identical, yet each pair is maintained twice. A one-sided tweak silently desyncs two screens. Share one type (and one parser) per pair, or alias the second name to the first.

### [M] Do not store world elevation range in UI tile style
`include/ui/style/UiStyle.h:43`–`44`, `src/ui/style/UiStyle.cpp:84`–`85` — `minElevationMeters` / `maxElevationMeters` duplicate `min_elevation` / `max_elevation` from `config/worldGen/presets.json` (same ±4000 today). Tile fill remapping reads the UI copy (`TileRenderer` via `Style().tileRenderer`), so a world-gen or mod elevation change that does not update `style.json` silently wrong-colors the map. Keep only visual knobs in style; take the elevation domain from map/world config (or a single shared constants source).

### [L] Convention and hygiene items
- `include/ui/style/UiStyle.h:11`–`572` — nested style bags are config/POD structs but omit the required `_t` suffix (`LayoutsStyle`, `TileRendererStyle`, …); the aggregate should be `UiStyle_t` (or similar), not a class with all-public data (`574`–`621`).
- `include/ui/style/UiStyle.h:580`, `src/ui/style/UiStyle.cpp:762`–`765` — `IsLoaded()` is unused outside this TU; dead API next to `Get()`’s throw-on-unloaded path.
- `src/ui/style/UiStyle.cpp:21` — `ParseColor_` accepts arrays longer than 4 and ignores the extras (layouts require exact length 4 at `:36`); tighten to size 3 or 4 for consistency.

**Observed outside slice:**
- `docs/architecture/ui-system.md` — UiStyle / `config/ui/style.json` are absent from the UI architecture diagram despite being a cross-cutting UI dependency.
- `src/ui/commlinks/CouncilProposalsPopup.cpp`, `src/ui/world/ProbeActionPopup.cpp` — reuse `Style().productionSelectorPopup` instead of dedicated sections (call-site smell; may need new style keys here when fixed).
