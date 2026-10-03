---
name: Graphics assets roadmap
overview: High-level roadmap for extracting SMAC art into local PNGs, wiring config sprite paths, and rendering them — starting with world-map terrain, ending with CVR-baked unit facings and secret-project video.
todos:
  - id: phase1-terrain
    content: "Phase 1: extract_terrain.py + wire TileLayerResolver into TileRenderer with scaled sprites"
    status: pending
  - id: phase2-bases
    content: "Phase 2: harden extract_faction.py + draw base sprites on WorldDisplay"
    status: pending
  - id: phase3-icons
    content: "Phase 3: extract_icons.py (techs/facs/projs) + sprite_path on configs + UI wiring"
    status: pending
  - id: phase4-ui
    content: "Phase 4: extract_ui.py (chrome/cursors/thumbs) + style.json + view wiring"
    status: pending
  - id: phase5-units
    content: "Phase 5: extract_units.py (Units.pcx) + bake_cvr_units.py + UnitMarkerRenderer"
    status: pending
  - id: phase6-media
    content: "Phase 6: extract_media.py (FLC/WVE) + playback UI"
    status: pending
  - id: extract-orchestrator
    content: "Cross-phase: extract_all.py orchestrator that runs every family extractor into expected assets/ paths"
    status: pending
isProject: false
---

# Graphics assets roadmap

## Constraints and settled decisions

- **No shipped art**: the game binary and repo never redistribute SMAC assets. A player (or developer) with a legal install runs Python extractors that write PNGs/media into the paths configs already expect under `assets/`. Same model as [`extract_faction.py`](extract_faction.py).
- **Source of truth**: local SMAC install at `/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/drive_c/GOG Games/Sid Meier's Alpha Centauri/`.
- **Every phase ships an extractor**: renderer/config work is incomplete without a script that recreates that phase’s assets from the install into their expected paths. Detailed plans must list the script name, inputs, and output tree.
- **Runtime format**: PNG (RGBA, magenta keyed where needed). Keep the existing 2D path: `Graphics::LoadTexture` / `DrawSprite` — no atlas UV work until profiling says we need it.
- **Config hook**: continue the optional `sprite_path` string pattern already used by terrain/improvements ([`ImprovementConfig_t::spritePath`](include/game/map/ImprovementConfigParser.h)).
- **Units**: workshop units are Caviar `.cvr` voxels, not PCX. **Bake offline to facing PNG sheets** (option 1). `Units.pcx` covers static natives only.
- **Video**: secret projects are `.wve` (not AVI) under `movies/`. Separate later phase.
- **Phase 1 focus**: world-map terrain (this plan’s first detailed follow-up).

```mermaid
flowchart LR
  SMAC[SMAC install PCX/CVR/WVE]
  Extract[Python extract tools]
  Assets[gitignored assets/ PNGs]
  Config[config sprite_path]
  Render[TileRenderer WorldDisplay UI]
  SMAC --> Extract --> Assets
  Assets --> Config --> Render
```

## Current baseline

| Piece | State |
|---|---|
| [`Graphics`](include/graphics/Graphics.h) / SFML | Texture + sprite draw ready |
| [`TileRenderer`](src/ui/TileRenderer.cpp) | Elevation fills + optional PNG overlays; falls back when files missing |
| [`terrain.json`](config/terrain.json) / [`improvements.json`](config/improvements.json) | Already point at `assets/sprites/...` paths that do not exist yet |
| [`TileLayerResolver`](src/game/map/TileLayerResolver.cpp) | Layer model exists; **not** used by live `TileRenderer` |
| [`extract_faction.py`](extract_faction.py) | Faction sheet slicer only |

## Asset pipeline (shared across phases)

Grow extraction into a tool family (evolve from `extract_faction.py`). Each family extractor is a **phase deliverable**, not a follow-up polish item.

- Shared PCX helpers: paletted load, magenta key (index 255), guide-box crops, contact sheets, `--game-dir`.
- Default game-dir override for this machine; keep Windows default as fallback.
- Output under `assets/` (gitignore extracted binaries; keep scripts + region tables in repo).
- **`extract_all.py` orchestrator** (cross-phase todo): runs every family extractor so a fresh checkout + SMAC install can populate the full expected `assets/` tree in one command. Grow it as each phase lands its script.

Document layout discoveries next to each extractor (region tables are reverse-engineered; SMAC has no sprite-index TXT).

| Phase | Extractor (in-repo) | Typical outputs |
|---|---|---|
| 1 Terrain | `extract_terrain.py` | `assets/sprites/landforms/…`, bonuses, forest, etc. |
| 2 Bases | `extract_faction.py` (existing; harden) | `assets/factions/<name>/bases/…`, `colors.json` |
| 3 Icons | `extract_icons.py` | `assets/sprites/techs/…`, `facilities/…`, `projects/…` |
| 4 UI | `extract_ui.py` | `assets/ui/…` chrome, cursors, notice thumbs |
| 5 Units | `extract_units.py` + `bake_cvr_units.py` | `assets/units/natives/…`, `assets/units/baked/…` |
| 6 Media | `extract_media.py` | `assets/media/flc/…`, `assets/media/movies/…` |
| All | `extract_all.py` | invokes the above into the expected layout |

## Phase order

### Phase 1 — World map terrain (next detailed plan)

**Goal:** map tiles look like SMAC land/sea/fungus/forest/moisture/rockiness instead of colored rects.

**Source sheets:** `texture.pcx` (base land/sea/fungus/river/road/cliffs), then `ter1.pcx` / `ter1wreck.pcx` (improvements, bonuses, monolith). `S#L#C#.pcx` are orbital planet art, not map tiles — out of Phase 1.

**Elevation (settled for Phase 1):** not a `TileLayer`. SMAC shades height/depth by palette offset at draw time and uses cliff-edge sprites for neighbor slopes. We keep continuous meters and apply an elevation/fog **tint** when drawing terrain sprites; cliff-edge compositing is deferred (needs slope rules on our square viewport). Details in the Phase 1 plan.

**Extractor + renderer:** see Phase 1 detailed plan (`extract_terrain.py`, `TileLayerResolver` draw path, scaled+tinted sprites).

Phase 1 deliberately skips bases, units, and UI chrome.

### Phase 2 — Faction bases on the map

**Extractor:** harden [`extract_faction.py`](extract_faction.py) (game-dir defaults, `--all`, gitignore `assets/factions/`); ensure output paths stay the contract for map drawing. Hook into `extract_all.py`.

**Renderer/config work:**

- Replace yellow base-name-only markers in [`WorldDisplay`](src/ui/world/WorldDisplay.cpp) with size/defense/water base sprites from `assets/factions/<id>/bases/…`, using faction colors from `colors.json`.
- Name labels stay as overlay text.

### Phase 3 — Content icons (tech / building / project)

**Extractor:** `extract_icons.py` — 1:1 convert `techs/techNNN.pcx`, `facs/facNNN.pcx`, `projs/projNNN.pcx` → the PNG paths written into config. Hook into `extract_all.py`.

**Renderer/config work:**

- Add `sprite_path` to building / tech / secret-project configs + parsers (`KnownBuildingKeys_` for buildings).
- Wire research / base / production UI placeholders to those icons.

### Phase 4 — UI chrome

**Extractor:** `extract_ui.py` — slice/convert `iface.pcx`, `console*.pcx`, `artbox*`, cursors, `*_sm` notice thumbs into `assets/ui/…`. Hook into `extract_all.py`.

**Renderer/config work:**

- Point [`config/ui/style.json`](config/ui/style.json) (and view code) at chrome sprites; keep rect+text fallbacks.

### Phase 5 — Units (natives + CVR bake)

**Extractors:**

- `extract_units.py` — slice `Units.pcx` → native/static unit PNGs.
- `bake_cvr_units.py` — offline CVR bake (build on CVR-Colorizer / format docs): compose chassis+weapon parts, render N facings (and faction `vehicle_color` recolor), write PNG sheets under `assets/units/baked/…`.
- Both registered in `extract_all.py` (bake may be opt-in / slower).

**Renderer/config work:**

- `sprite_path` / facing table on [`native_units.json`](config/native_units.json); draw in [`UnitMarkerRenderer`](include/ui/world/UnitMarkerRenderer.h).
- Per-component art keys (chassis primary; weapon overlay if needed) + facing index from unit facing state (add facing to unit model if missing — leave a TODO until that phase confirms the data field).
- Replace cyan letter-chips when sprites exist; keep chips as fallback.

### Phase 6 — Motion media

**Extractor:** `extract_media.py` — FLC → PNG sequences (or playable intermediate); WVE → playable files the engine can open; map stems via `movlist.txt` / `movlistx.txt`. Hook into `extract_all.py`.

**Renderer/config work:**

- Playback hooks for leader/diplomacy/FX and secret-project completion UI.

## Cross-cutting rules

- Extraction scripts live in-repo; **extracted art does not**. The shipped product assumes the player ran the extractors against their own SMAC install.
- Each phase’s definition of done includes: **extractor script + assets land in expected paths + game reads those paths**. No phase is “renderer only.”
- Never invent SMAC art in C++; missing asset → existing procedural/placeholder path.
- Prefer config paths over hard-coded texture ids.
- Each phase gets its own detailed plan before implementation (and that plan must name the extractor).
- Architecture docs update when the live draw path changes (Phase 1+).

## Immediate next step

Write the **Phase 1 detailed plan**: reverse-engineer `texture.pcx` / `S#L#C#` layouts against our landform/moisture/rockiness/vegetation model, define exact PNG output paths for `extract_terrain.py`, and list the concrete `TileRenderer` + config edits.
