# Graphics System Architecture

```mermaid
graph TB
    subgraph "Graphics Interface"
        Graphics[Graphics<br/>(abstract base class)]
        Methods[Virtual Methods:<br/>PumpEvents()<br/>Clear()<br/>Display()<br/>LoadTexture()<br/>DrawSprite()<br/>DrawTileSprite()<br/>FillTileShape()<br/>DrawText()<br/>SetMouseCursor()<br/>ResetMouseCursor()]
    end

    subgraph "SFML Implementation"
        SFMLGraphics[SFMLGraphics]
        SFMLWindow[sf::RenderWindow]
        SFMLFont[sf::Font]
        TextureMap[unordered_map<string, sf::Texture>]
        EventProcessing[PumpEvents → PlatformEventQueue]
    end

    subgraph "Null Implementation"
        NullGraphics[NullGraphics]
        ConsoleOutput[Console logging only<br/>No rendering]
    end

    subgraph "Factory"
        Factory[CreateGraphics()]
        CompileFlag[USE_SFML<br/>compile-time flag]
    end

    subgraph "Map drawing"
        SpriteLibrary[SpriteLibrary<br/>Ensure(path)]
        MapRenderer[MapRenderer<br/>Engine-owned]
        TileRenderer[TileRenderer<br/>one tile]
        FactionBaseArtCache[FactionBaseArtCache]
    end

    subgraph "Dependencies"
        KeyMapping[KeyMapping]
        PlatformEventQueue[PlatformEventQueue<br/>owned by Engine]
    end

    Graphics --> Methods
    SFMLGraphics -.->|implements| Graphics
    NullGraphics -.->|implements| Graphics

    SFMLGraphics --> SFMLWindow
    SFMLGraphics --> SFMLFont
    SFMLGraphics --> TextureMap
    SFMLGraphics --> EventProcessing
    SpriteLibrary --> TextureMap
    MapRenderer --> TileRenderer
    MapRenderer --> FactionBaseArtCache
    TileRenderer --> SpriteLibrary
    FactionBaseArtCache --> SpriteLibrary

    EventProcessing --> KeyMapping
    EventProcessing --> PlatformEventQueue

    Factory -->|if USE_SFML defined| SFMLGraphics
    Factory -->|if USE_SFML not defined| NullGraphics
    Factory --> CompileFlag

    NullGraphics --> ConsoleOutput

    style Graphics fill:#bbf,stroke:#333,stroke-width:4px
    style SFMLGraphics fill:#bfb,stroke:#333,stroke-width:2px
    style NullGraphics fill:#fbb,stroke:#333,stroke-width:2px
    style Factory fill:#ff9,stroke:#333,stroke-width:2px
```

## Component Overview

### Graphics (Abstract Base Class)
- **Purpose**: Defines the interface for graphics rendering operations
- **Virtual Methods**:
  - `PumpEvents()`: Drain the window's event queue into the `PlatformEventQueue` this backend was constructed with. Deliberately **not** part of `Display()`: pumping used to hang off the render call, which made rendering a prerequisite for receiving a keystroke and gave a draw call hidden I/O side effects.
  - `Clear()`: Clear the render surface
  - `Display()`: Present the rendered frame (SFML also applies `framerateLimit` here)
  - `PaceFrame()`: Sleep to honor `framerateLimit` without presenting — used when UIManager skips a quiet frame
  - `LoadTexture(id, path)`: Load a texture from file
  - `UpsertTextureRGBA(id, width, height, rgba)`: Create or replace an RGBA8 texture from tightly packed pixels (minimap terrain cache)
  - `DrawSprite(textureId, x, y)` / `DrawSprite(..., destWidth, destHeight)` / `DrawSprite(..., destWidth, destHeight, tint)`: Draw a sprite at position, optionally scaled and color-multiplied
  - `DrawTileSprite(textureId, paletteId, shape)`: Draw palette-index art (grey is the `palette.pcx` index, alpha is coverage) the way SMAC does. The texture's inscribed diamond maps onto a `TileShape_t` (centre and W/N/E/S corners, each with a position and a `shade` in palette steps) as four triangles around the centre. Each pixel takes the palette entry at its index plus the interpolated shade rounded to a whole step, within the art range 0–235. `SFMLGraphics` does this in a fragment shader (the shade travels in the vertex colour) and throws at startup without shader support. Edge pixels sample just inside the texture's diamond, so shapes that share vertices meet without gaps (terrain tiles, raised relief, water depth shading)
  - `FillTileShape(shape, color)`: Fill a shape's four triangles with one color (tile fill, fog haze, shroud)
  - `DrawText(text, x, y, size)`: Draw text at position
  - `MeasureTextWidth(text, size)`: Width of the string at the given font size (SFML uses glyph bounds; headless/test backends approximate)
  - `DrawRect(x, y, width, height, color, thickness)`: Draw an outline rectangle (negative thickness draws inward)
  - `DrawFilledDiamond` / `DrawDiamond`: Isometric tile footprint whose AABB is `(x, y, width, height)`
  - `SetMouseCursor(path, hotspotX, hotspotY)`: Apply a custom OS cursor from an image file. Empty path or load failure leaves the current cursor and returns false; the backend keeps the cursor object alive until the next set/reset
  - `ResetMouseCursor()`: Restore the system arrow cursor

### SFMLGraphics
- **Purpose**: SFML-based graphics implementation
- **Components**:
  - `sf::RenderWindow`: SFML render window, sized/titled/FPS-capped from `GraphicsConfig_t`
  - `sf::Font`: opened from the first usable path in `GraphicsConfig_t::fontPaths`
    (and `user_settings.json` `graphics.font_paths`). **Throws if none opens** — the
    entire UI is text and rectangles, so "no font" would present a black window with no
    diagnostic. Defaults try `assets/ui/fonts/arialn.ttf` first (SMAC Arial Narrow from
    `python extract_fonts.py`; gitignored under `assets/ui/fonts/`), then system
    DejaVu/Liberation. UI text is solid RGBA via `DrawText` — not the terrain palette
    shader.
  - `unordered_map<string, sf::Texture>`: Texture cache; `LoadTexture` replaces an existing id
  - `PumpEvents()`: Translates SFML events and pushes them onto the shared `PlatformEventQueue`
- **Dependencies**:
  - Uses `KeyMapping` (SFML half in `SfmlKeyMapping.cpp`) to convert SFML keys to `Key_t`
  - Writes to `PlatformEventQueue`; it never names an `Input` implementation
- **Window close**: a close request is *recorded* on the `PlatformEventQueue`, not acted on. `Engine::GameLoop_` takes it and calls `UIManager::RequestExit()`, so every quit path sets the same flag. What closing means is the engine's decision, not the backend's.

### NullGraphics
- **Purpose**: Substitutable no-op backend for headless runs
- **Behavior**:
  - Draw calls do nothing and **report success**: a caller should not have to special-case headless to tell "did nothing" from "went wrong"
  - `PumpEvents()` writes nothing, so with no other producer the queue stays empty and `Input` polls empty — never blocking
  - `Display()` presents; `PaceFrame()` sleeps to `GraphicsConfig_t::framerateLimit`. SFML's
    `setFramerateLimit` paces presented frames; skip-redraw paths call `PaceFrame()` instead.
    Without an equivalent here a headless run would spin at 100% CPU
  - Reports the same window size as the SFML backend, from the shared `GraphicsConfig_t`
  - Used when `USE_SFML` is not defined

### CreateGraphics() Factory
- **Purpose**: Factory function to create appropriate graphics implementation
- **Selection**: Based on `USE_SFML` compile-time flag
  - If defined: Returns `SFMLGraphics`
  - If not defined: Returns `NullGraphics`

### SpriteLibrary
- **Purpose**: Loads each sprite file path into the `Graphics` backend at most once
- **File**: `ui/SpriteLibrary.h`, `ui/SpriteLibrary.cpp`
- **Construction**: `Engine` builds one library over its `Graphics` instance, with a
  `FileExists_t` callback (`std::filesystem::exists` in production)
- **API**: `Ensure(path)` returns true when the texture is loaded and usable; logs once and
  returns false when the file is missing, empty, or the backend load fails (subsequent calls
  remember failure without re-logging)
- **Consumers**: `TileRenderer` (terrain and object sprites) and `FactionBaseArtCache` (faction
  base sheets and building map overlays), both owned by `MapRenderer`. Later UI renderers are
  expected to load through the same library rather than calling `LoadTexture` directly

## UI Components

UI components use the Graphics interface to render game information.

### PopulationDisplay
- **Purpose**: Displays current population and per-pop type breakdown
- **File**: `ui/PopulationDisplay.h`, `ui/PopulationDisplay.cpp`
- **Dependencies**: EventBus (for population change events), Graphics
- **Methods**:
  - `Render(x, y)`: Render at specified position
  - `SetPopulation()`: Set the population manager to display
  - `SetCurrentPop()`: Set population directly

### MapRenderer
- **Purpose**: The one path map tiles take to the screen. `WorldDisplay`, `BaseWorkableAreaDisplay`
  and `LocationPanel` draw through `Render`; `MinimapDisplay` takes its colours from `TileColor`.
  Nothing else calls `TileRenderer`.
- **File**: `ui/MapRenderer.h`, `ui/MapRenderer.cpp`
- **Lifetime**: One instance owned by `Engine`, built over the `SpriteLibrary` and `GameState` with
  the `tile_renderer` and `map_renderer` style blocks. It constructs its `TileRenderer` and one
  `FactionBaseArtCache` (one `colors.json` cache for every display). `ViewFactory` passes
  `MapRenderer&` to `WorldView` and `BaseView`.
- **Inputs**: a display describes what to draw; the displays differ only here:
  - placed tiles (`PlacedTile_t`: tile + shape), back to front;
  - a `MapAppearance`: whose knowledge the map shows and whether remembered tiles are fogged;
  - a `MapContent_t`: which bases (`showsBase`) and units (`showsUnit`) to show, and the selected
    unit. An empty filter shows none.
- **Reads from `GameState`**: base sprite sizes and overlay channels, the map display's ocean grid,
  live tile yields (farm structure rows), and `FindBaseAt`.

```mermaid
flowchart LR
    WD[WorldDisplay<br/>VisibleTiles · Fogged(player) · all bases, visible units]
    BW[BaseWorkableAreaDisplay<br/>radius · Clear(base faction) · all bases]
    LP[LocationPanel<br/>one tile · Clear(player)]
    MM[MinimapDisplay]
    MR[MapRenderer]
    TR[TileRenderer]
    WD -->|Render| MR
    BW -->|Render| MR
    LP -->|Render| MR
    MM -->|TileColor| MR
    MR -->|RenderTerrain / RenderObjects / FillColor| TR
```

- **Render order** (SMAC's): per tile, `TileRenderer::RenderTerrain` → grid edges →
  `TileRenderer::RenderObjects`; then per tile its base (bare sprite and building overlays seated
  at `FootprintOrigin` with `base_sprite_overhang_ratio`, then its name), never on a shrouded
  tile; then per tile the units the content shows, side by side, the selected one bordered.
  `Render` returns each drawn unit's marker rect. Displays draw their own overlays afterwards
  (path preview, yield numbers).
- **Grid**: one rule for every display. A placed tile draws its NW and NE edges where a map
  neighbour exists, and its SE and SW edges where the neighbour exists but is not placed, so each
  shared edge draws once (from the front tile) and a partial scene keeps a closed outline.
  `grid_land_color` between land tiles and next to shroud (so the grid does not reveal
  coastlines); edges touching water draw in `grid_water_color` only with the map display's
  `ocean_grid`.
- **Bases**: `FactionBaseArtCache` resolves the bare sprite and building map overlays; the name
  label uses `faction_text_color_primary` (else `faction_color_primary`, else `base_name_color`)
  and is cut down with `MeasureTextWidth` to `base_name_width_ratio`. A base whose art does not
  load shows only its name.

### WorldDisplay / MapViewport
- **Purpose**: Displays the world map as a SMAC-style rectangular brick of 2:1 diamonds with
  faction base sprites and name labels
- **File**: `ui/world/WorldDisplay.h`, `ui/world/MapViewport.h`, `ui/MapRenderer.h`,
  `ui/world/FactionBaseArt.h`
- **Model vs presentation**: `WorldMap` uses SMAC coordinates (wrap-X, even parity). Lattice geometry
  lives in `MapUtils`; screen placement lives in `MapViewport` — tile `(x, y)`'s footprint at
  `((x − camX)·½w, (y − camY)·½h)`, with `PixelOriginOf` / `PixelCenterOf` /
  `WorldCoordsAtPixel` / row-ordered `VisibleTiles`.
- **Relief**: `MapViewport::SetRelief` (from `GameSettings::GetMapDisplay()`, set every frame)
  raises each tile's centre and corners with `TileRelief` (`ui/TileRelief.h`), SMAC's vertex
  lift ([smac-palette-lighting.md](../thinker/smac-palette-lighting.md), "Relief"). Per-tile
  lifts and slope shades are cached in vectors keyed on `WorldMap::GetAppearanceRevision()`,
  the relief mode, and the relief style; `ShapeAt_`, `PixelOriginOf`, and `MaxLiftPixels_` read
  the cache and only rebuild when one of those inputs changes:
  - A land centre lifts `elevation / level_meters` levels (`smooth`) or whole levels
    (`stepped`), `lift_per_level_ratio` of a tile width per level; `flat` turns relief off.
  - A corner takes the mean of its four tiles, or stays at sea level when one is water or off
    the map. Water does not lift.
  - `VisibleTiles` lists each tile's raised, slope-shaded `TileShape_t` as a `PlacedTile_t`,
    reaching far enough down to include tiles raised into view.
    `PixelOriginOf` / `PixelCenterOf` report the footprint seated at the mean of its four
    corner lifts, as SMAC's `MapWin_tile_to_pixel` seats everything on a tile, so units, bases
    and markers sit where the tile's objects do, and `WorldCoordsAtPixel` picks the frontmost raised shape under the pixel.
  - Slope shades follow SMAC's facet shading in palette steps (lighter facing the screen's
    lower right). A slope gets full shade once a corner rises
    `full_shade_rise_meters` above the centre and gentler ones shade in proportion. Stepped
    corners rise in whole quarter levels, so a value up to a quarter level keeps SMAC's results.
  - Land also lightens `altitude_light_steps` steps per level above sea level, so higher ground
    reads brighter (not in SMAC; 0 turns it off).
- **Grid**: drawn by `MapRenderer` between terrain and objects (see its grid rule). The settings
  panel's Map Display rows switch the relief (a Choice row: Smooth, Stepped, Flat) and the
  ocean grid.
- **Methods**:
  - `Render(rGraphics)`: Painter’s-algorithm pass over visible diamonds from the stored camera
  - `SetSelectedUnit(pUnit)`: Highlight the player's selected unit
  - `GetViewport().SetCamera(tileX, tileY)`: Anchor for the brick projection (map units)
- **Tile drawing**: `WorldDisplay::Render` hands `MapViewport::VisibleTiles()` to
  `MapRenderer::Render` with `MapAppearance::Fogged(map, player)`, every base, and the units the
  player can see (`IsUnitVisibleTo`, or listed by bombard playback), then draws the path preview
  on top. Each diamond gets `FillColor`, terrain layers, grid edges, then object sprites. Occupant art comes from each config entry's `art` block (see map-system
  docs); palette terrain draws with `DrawTileSprite`, water landforms with depth shading, coast
  per corner from `Rainfall.pcx` extracts
  ([smac-coastline-rainfall.md](../thinker/smac-coastline-rainfall.md)). Object sprites (TER1
  100×62 over a 100×50 footprint) use `art.overhang` and seat at `SeatOf` the raised shape.
  Missing configured terrain art draws nothing (the fill shows); missing object art draws the
  magenta/black checker from `tile_renderer` style keys. Art is not shipped; run
  `extract_terrain.py` against a local SMAC install to populate `assets/sprites/`.
- **Unit markers**: `MapRenderer` returns where it drew each unit; `WorldDisplay::MarkerRectOf`
  serves them to the combat hit overlays.
- **Hit-testing**: `WorldView` calls `MapViewport::WorldCoordsAtPixel` (map-unit diamond under
  the pixel, then raised tiles in front). `BaseWorkableAreaDisplay` hit-tests its own diamonds
  with `ShapeContains` from `ui/TileShapeGeometry.h` (shared footprint helpers with the viewport).
- **Bases** (drawn by `MapRenderer`): `FactionBaseArtCache` uses `assets/factions/<faction.id>/` when that directory
  exists (`extract_faction.py` maps SMAC `.pcx` stems like `gaians`/`univ` onto config ids).
  Draws bare land/water bases plus `colors.json`. Base sheet shadows use the same index-246
  bake as ter1 objects (`extract_pcx_common.apply_shadow`: land alpha 54, water 21). Building
  map overlays come from building config (`map_overlay` paths with `{faction}` / `{size}`),
  resolved through `map_overlay_channels.json` (channel exclusivity by priority; default draw
  layer per channel; optional per-building layer override). Perimeter/Tachyon art is a
  pixel-diff overlay on the bare base, not a full fortified sprite. Size stages come from
  `config/base_sprite_sizes.json` (open-ended `size_stages` + optional `stage_bump_buildings`).
  Missing sizeN art falls back to sizeN−1…size1 with a stderr warning (bare bases and building
  overlays alike). Sprites seat like other map objects at `FootprintOrigin` (corner-mean origin,
  `base_sprite_overhang_ratio` ≈ (75−62)/62). Missing all sizes keeps the name-only marker, on the
  world map and in the base view alike.
- **Architecture Note**: `WorldDisplay` reads the map and bases live from `GameState` during
  render (no per-frame base-info DTO). Base-at-tile clicks go through
  `GameState::FindBaseAt`, owned by the model rather than `WorldView`.

### TileRenderer
- **Purpose**: Paints one tile for `MapRenderer`, as the `MapAppearance` shows it
- **File**: `ui/TileRenderer.h`, `ui/TileRenderer.cpp`
- **Lifetime**: Constructed and owned by `MapRenderer` with the session's `SpriteLibrary&` and
  `TileRendererStyle_t` from `UiStyle`. Coast sprite path tables (2 parts × 4 corners × 8 cases)
  are built in the constructor from `coast_sprite_dir` in style.
- **Footprint helpers**: `ui/TileShapeGeometry.h` — `k_IsoHeightRatio`, `PlacedTile_t`,
  `FlatTileShape`, `ShapeContains`, `SeatOf` (mean of the four corners), `FootprintOrigin` (the
  flat footprint's top-left at that seat). Displays place a `TileShape_t` from `MapViewport`
  (raised and shaded) or a flat diamond for the base radius and the location preview.
- **MapAppearance**: Every method takes `const MapAppearance&` and nothing else about sight.
  `CoverOf` picks shroud, fog or clear; occupant lists and neighbor walks for autotile and road
  links use `OccupantsOf`; elevation, surface, coast, and water depth stay on the live `Tile`.
  See `ui/world/MapAppearance.h` and map-system `FactionTileMemory`.
- **Draw order** (`MapRenderer` draws the grid between the halves):
  1. `FillColor` — `shroud_color` under shroud (and nothing else is drawn); otherwise the
     elevation gradient on water/land or an occupant `fill_color`, dimmed under fog
  2. Terrain layers by `ArtLayer_t`: landform, moisture (or farm `ground`), rockiness, landmark,
     vegetation — each occupant with matching `art.layer` on the tile
  3. Coast overlay (live map geometry)
  4. River layer, then link networks (`links` tile sets on the road layer)
  5. Fog haze under fog
  6. `RenderObjects` (nothing under shroud): `object` layer for terrain occupants then
     improvements, skipping ids in any present occupant's `hides`; yield rows follow the
     `YieldLookup_t` (`MapRenderer` passes live yields)
- **Palette art**: terrain and coast cells are palette indices drawn with `DrawTileSprite` and
  the style's palette texture; land uses relief vertex shades (fog replaces them with
  `fog_land_shade` steps). Water landforms use `WaterShading` (`ui/WaterShading.h`) with
  per-landform `depth_shade` on the occupant art and style keys for deep/shelf swap and coast
  ranges.
- **Sprites**: `TryDrawSprite_` / `TryDrawTileSprite_` call `SpriteLibrary::Ensure` before draw.
- **Missing art**: configured terrain paths that fail to load draw nothing (fill remains).
  Configured object paths that fail draw `DrawMissingArt_` — a 2×2 checker at `SeatOf` using
  `missing_art_color`, `missing_art_alt_color`, and `missing_art_size_ratio`. No procedural
  moisture, rockiness, or river fallbacks.
- **Free helpers**: `PickSpriteIndex` / `PickSpritePath` for stable variant choice from tile
  coords and occupant id.

### MapAppearance
- **Purpose**: The map as one viewer sees it — what covers each tile and which occupants it
  shows (live on visible tiles, remembered on explored tiles out of sight)
- **File**: `ui/world/MapAppearance.h`, `ui/world/MapAppearance.cpp`
- **API**: `MapAppearance::Fogged(world, pViewer)` (remembered tiles under fog: world map,
  minimap) and `MapAppearance::Clear(world, pViewer)` (remembered tiles clear: location
  preview, base view); a null viewer is live and clear everywhere. `CoverOf(tile)` →
  `TileCover_t::Shroud` (never explored) / `Fog` (out of sight, `Fogged` only) / `None`;
  `OccupantsOf(tile)`.

### BaseWorkableAreaDisplay
- **Purpose**: Displays the workable area of a base as a brick of 2:1 diamonds matching the
  world map (lattice `(p, q)` → map `(p − q, p + q)`), using `FlatTileShape`
- **File**: `ui/base/BaseWorkableAreaDisplay.h`, `ui/base/BaseWorkableAreaDisplay.cpp`
- **Dependencies**: Graphics, WorldMap, Base, WorkerAssignmentManager, `MapRenderer`
- **Layout**: Cluster sized to the Euclidean radius-2 disk, placed back to front; base diamond at
  the centre; clicks use `ShapeContains` (front-most diamond wins).
- **Paint**: `MapRenderer::Render` like WorldDisplay, with `MapAppearance::Clear` for the base's
  faction and every base on its tiles (same grid rule, base art and name), then yield triples on
  surrounding tiles; worked green / unworked white / unavailable dim

## View System

The Engine manages a `ViewMode` state machine with two views:

### World View (default)
- Renders `WorldDisplay` with the full tile grid and faction base sprites / name labels
- **Mouse click on base tile**: Opens Base View for that base
- **Mouse click elsewhere**: Shows clicked tile coordinates
- **Enter**: Processes a turn
- **Escape**: Exits the game

### Base View
- Renders `BaseWorkableAreaDisplay` centered on the active base
- Shows the base name at the top
- **Mouse click on workable tile**: Shows clicked tile coordinates
- **Escape**: Returns to World View
