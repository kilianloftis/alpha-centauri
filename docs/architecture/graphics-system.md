# Graphics System Architecture

```mermaid
graph TB
    subgraph "Graphics Interface"
        Graphics[Graphics<br/>(abstract base class)]
        Methods[Virtual Methods:<br/>PumpEvents()<br/>Clear()<br/>Display()<br/>LoadTexture()<br/>DrawSprite()<br/>DrawDiamondSprite()<br/>DrawText()<br/>SetMouseCursor()<br/>ResetMouseCursor()]
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
  - `DrawSprite(textureId, x, y)` / `DrawSprite(..., destWidth, destHeight)` / `DrawSprite(..., destWidth, destHeight, tint)`: Draw a sprite at position, optionally scaled and color-multiplied (fog on map tiles)
  - `DrawDiamondSprite(textureId, x, y, destWidth, destHeight, tint)`: Draw the texture's inscribed diamond onto the destination diamond as four triangles around the centre, with a `DiamondTint_t` color multiply at the centre and each corner interpolated across it. Edge pixels sample just inside the texture's diamond, so neighboring diamonds meet without gaps (terrain tiles, water depth shading)
  - `DrawText(text, x, y, size)`: Draw text at position
  - `DrawRect(x, y, width, height, color, thickness)`: Draw an outline rectangle (negative thickness draws inward)
  - `DrawFilledDiamond` / `DrawDiamond`: Isometric tile footprint whose AABB is `(x, y, width, height)`
  - `SetMouseCursor(path, hotspotX, hotspotY)`: Apply a custom OS cursor from an image file. Empty path or load failure leaves the current cursor and returns false; the backend keeps the cursor object alive until the next set/reset
  - `ResetMouseCursor()`: Restore the system arrow cursor

### SFMLGraphics
- **Purpose**: SFML-based graphics implementation
- **Components**:
  - `sf::RenderWindow`: SFML render window, sized/titled/FPS-capped from `GraphicsConfig_t`
  - `sf::Font`: opened from the first usable path in `GraphicsConfig_t::fontPaths`. **Throws if none opens** — the entire UI is text and rectangles, so "no font" would present a black window with no diagnostic.
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

### WorldDisplay / MapViewport
- **Purpose**: Displays the world map as a SMAC-style 2:1 isometric diamond grid with base markers
- **File**: `ui/world/WorldDisplay.h`, `ui/world/MapViewport.h`
- **Model vs presentation**: `WorldMap` stays square (neighbors, wrap-X, pathfinding). All isometric
  math lives in `MapViewport` (`PixelOriginOf` / `PixelCenterOf` / `WorldCoordsAtPixel` /
  depth-ordered `ForEachVisibleTile`).
- **Methods**:
  - `Render(rGraphics)`: Painter’s-algorithm pass over visible diamonds from the stored camera
  - `SetSelectedUnit(pUnit)`: Highlight the player's selected unit
  - `GetViewport().SetCamera(tileX, tileY)`: Anchor tile for the isometric projection
- **Tile drawing**: Each diamond is painted by `TileRenderer` — elevation-colored fill, then
  `ResolveTileLayers` sprites scaled to the diamond AABB. Land art keeps its sheet colors and
  water art is shaded per vertex by depth, as in SMAC
  ([smac-palette-lighting.md](../thinker/smac-palette-lighting.md)). Occupants
  list `sprite_paths` per tile surface (`land` / `sea`), and the renderer's `PickSpritePath`
  chooses one from a hash of tile coordinates and content id (stable across save/load; no
  per-tile variant field). Or they name a `sprite_tiles` set, and the renderer draws the
  cell for the tile's neighbor mask (moisture, forest, fungus, jungle, rivers). Object
  sprites (tile bonuses, monolith: `ter1.pcx` 100×62 over a 100×50 footprint) set
  `sprite_overhang_ratio` and are drawn on the tile's footprint, reaching above it. Their
  shadows are partly transparent black, so they darken the terrain underneath as SMAC's
  shadow table does (`extract_terrain.py` turns ter1.pcx's shadow index 246 into black at
  alpha 54 on land sprites, 21 on sea sprites). The
  tile's single moisture cell fills the diamond and fades out toward drier land.
  Land next to water then gets SMAC's coast: per diamond corner, ocean and a shore band
  baked from `Rainfall.pcx`
  ([smac-coastline-rainfall.md](../thinker/smac-coastline-rainfall.md)). Rivers are a tile
  layer drawn above the coast. Missing PNGs fall back to procedural moisture, rockiness and
  river cues. Art is not shipped; run `extract_terrain.py` against a local SMAC install to
  populate `assets/sprites/`. Terrain cells are baked to 112×56 diamonds. Tile sets go to
  `sprites/landforms/<set>/<mask>.png`: blob sets get 47 masks, edge sets 16.
  Rolling/rocky are keyed overlays, and `sprites/coast/` holds the coast overlays.
  `--contact-sheet` also writes `_tiles_contact_sheet.png` to check the tile sets.
  Elevation perspective is a follow-on.
- **Hit-testing**: `WorldView` calls `MapViewport::WorldCoordsAtPixel`. Orthogonal
  `TileHitTester::HitTestWorldGrid` remains for non-iso grids; base workable area stays orthogonal.
- **Architecture Note**: `WorldDisplay` reads the map and bases live from `GameState` during
  render (no per-frame base-info DTO). Base-at-tile clicks go through
  `GameState::FindBaseAt`, owned by the model rather than `WorldView`.

### TileRenderer
- **Purpose**: Shared map-cell paint for the world map, location preview, and similar views
- **File**: `ui/TileRenderer.h`, `ui/TileRenderer.cpp`
- **Footprint**: 2:1 diamond (`size` = width, height = size / 2)
- **Elevation**: continuous meters → fill gradient and water depth shading (not a `TileLayer`).
  Land art draws untinted
- **Water shading**: `WaterShading` (`ui/WaterShading.h`) gives a water tile's centre the
  shade of its own depth and each corner the shade of the average depth of the tiles that
  share it (land at ocean level, off-map rows left out, x wraps). `water_shading.depth_shades`
  splits the map's floor up to ocean level into equal bands, deepest first (SMAC's table).
  `water_shading.tints` maps each water landform id to a color multiply per shade, derived from
  the `palette.pcx` water ramp. The Landform sprite is drawn with `DrawDiamondSprite` at the
  full tile rect
- **Fog**: fogged land multiplies its terrain art by `fog_terrain_dim_ratio` (SMAC's two
  palette steps); water art is not dimmed. Before the Improvement layer a fogged tile gets a
  `fog_haze_color` diamond (the average of SMAC's black scanlines). The Improvement layer and the
  improvement and feature sprites draw untinted on top. Fills, procedural cues and the minimap
  use `fog_fill_dim_ratio`
- **Variants**: `sprite_paths.land` / `.sea` by tile surface + `PickSpriteIndex` /
  `PickSpritePath` (coord + id hash); `sprite_overhang_ratio` lifts object sprites above
  the footprint
- **Tile sets**: `sprite_tiles` replaces `{mask}` in the surface's pattern with
  `ResolveTileMask` (`ui/TileAutotile.h`): an `edges` set takes 4 edge-neighbor bits, a `blob`
  set adds the corner neighbors between two matching edges (47 distinct masks). The Moisture
  layer matches water and neighbors at least as wet, every other layer neighbors with the same
  occupant.
  Without a `WorldMap` every set draws mask 0
- **Moisture**: one base cell per land tile at the full tile rect; no stacking or insets
- **Diamonds**: every terrain layer and the coast draw with `DrawDiamondSprite`, so tiles meet
  edge to edge with no fill between them. Object sprites (the Improvement layer, improvement
  and feature passes) draw as rects with their overhang
- **Edge insets**: rockiness overlays only. `MatchRockinessEdges` + `DestRectForEdgeInsets`
  (`TileSpriteEdgeInset`; a scaled diamond that stays inside the tile, flush on matched edges
  where it can); style key `sprite_overlay_edge_inset_ratio`
- **Coast**: `CoastOverlay` gives each diamond corner of a land tile a 3-bit water mask (edge
  neighbors are orthogonal, the corner neighbor diagonal) and SMAC's odd-row alternate for
  all-water corners. Drawn after Vegetation and before River as
  `<coast_sprite_dir>/{water,shore}_<w|n|e|s>_<mask>[_alt].png` at the tile rect. Water is
  drawn with `DrawDiamondSprite`, shaded from the land tile's own vertex depths with the
  `water_shading.coast_tints` table, so it meets the neighboring water at their shared corners;
  the shore takes the land's terrain tint. Needs a `WorldMap`; style key `coast_sprite_dir`
- **Rivers**: the River layer draws its `edges` cell; without art, lines run from the tile
  centre to each connected edge's midpoint (`GetRiverConnections`), or a short cross with no
  connection. Style keys `river_color` / `river_line_thickness_ratio` under `tile_renderer`
- **Layers**: fungus wins vegetation; a landmark (Monsoon Jungle) draws on its own layer and
  is skipped by the feature-sprite pass

### BaseWorkableAreaDisplay
- **Purpose**: Displays the workable area of a base (21 tiles in 5x5 diamond pattern)
- **File**: `ui/BaseWorkableAreaDisplay.h`, `ui/BaseWorkableAreaDisplay.cpp`
- **Dependencies**: Graphics, WorldMap, Base, WorkerAssignmentManager
- **Methods**:
  - `Render(x, y, tileSize)`: Render the workable area centered at position
  - `SetBase()`: Set the base to display workable area for
- **Tile Display Format**: Each tile shows `nutrients minerals energy`
- **Visual Indicators**: 
  - Worked tiles: Green text
  - Unworked tiles: White text
  - Base center: Yellow "BASE" label

### TileHitTester
- **Purpose**: Converts pixel coordinates to tile coordinates for orthogonal grids
- **File**: `ui/TileHitTester.h`, `ui/TileHitTester.cpp`
- **Dependencies**: None (stateless utility, all methods are static)
- **Methods**:
  - `HitTestWorldGrid(...)`: Orthogonal grid helper (world map uses `MapViewport::WorldCoordsAtPixel`)
  - `HitTestBaseWorkableArea(mouseX, mouseY, renderCenterX, renderCenterY, tileSize, baseX, baseY)`: Returns `optional<pair<int,int>>` tile coords for clicks on the base workable area (validates diamond pattern)
- **Usage**: Base workable area and similar orthogonal panels

## View System

The Engine manages a `ViewMode` state machine with two views:

### World View (default)
- Renders `WorldDisplay` with the full tile grid and yellow "BASE" labels
- **Mouse click on base tile**: Opens Base View for that base
- **Mouse click elsewhere**: Shows clicked tile coordinates
- **Enter**: Processes a turn
- **Escape**: Exits the game

### Base View
- Renders `BaseWorkableAreaDisplay` centered on the active base
- Shows the base name at the top
- **Mouse click on workable tile**: Shows clicked tile coordinates
- **Escape**: Returns to World View
