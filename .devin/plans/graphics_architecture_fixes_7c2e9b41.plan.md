---
name: Graphics architecture fixes
overview: Fix the review of 64ae149..9284349. Occupant art declares its own layer in one art block. A SpriteLibrary owns sprite loading and TileRenderer becomes an injected instance. Lattice geometry lives in one place. Relief is cached. Fogged tiles draw what the faction remembers. The rockiness inset, the procedural art cues and dead interface are removed.
todos:
  - id: small-cleanups
    content: "Remove Graphics::DrawFilledDiamond, the uncompiled improvement showcase in Engine.cpp, and tracked __pycache__ files"
    status: pending
  - id: delete-rockiness-inset
    content: "Confirm against SMAC, then draw rocky/rolling as full-tile cells and delete TileSpriteEdgeInset"
    status: pending
  - id: missing-art-policy
    content: "Missing art: no procedural cues; the fill shows under terrain and objects draw the checker"
    status: pending
  - id: lattice-geometry
    content: "Corner, edge and ring neighbor tables in MapUtils; DiamondValues_t; ui/TileShapeGeometry.h"
    status: pending
  - id: sprite-library
    content: "SpriteLibrary owned by Engine; TileRenderer becomes an instance; FactionBaseArtCache loads through it"
    status: pending
  - id: occupant-art
    content: "One art block per occupant with a declared layer; renderer buckets ForEachOccupant by layer; delete TileLayer/TileLayerResolver"
    status: pending
  - id: relief-cache
    content: "MapViewport caches per-tile lifts and shades keyed on the map's appearance revision"
    status: pending
  - id: measure-text
    content: "Graphics::MeasureText replaces the char-width-ratio style keys"
    status: pending
  - id: tile-memory
    content: "FactionTileMemory + MapAppearance: fogged tiles draw the occupants the faction last saw"
    status: pending
  - id: docs
    content: "Update graphics-system.md, map-system.md, ui-system.md and the assets roadmap"
    status: pending
isProject: false
---

# Graphics architecture fixes

Do the steps in the order listed. Each step leaves `./bd all` green. Shipping config and the
fixtures under `tests/fixtures/` migrate in the same step that changes a key. Nothing keeps an
old key alive.

## 1. Small cleanups (`small-cleanups`)

- Delete `Graphics::DrawFilledDiamond` from [Graphics.h](include/graphics/Graphics.h), both
  backends and [RecordingGraphics.h](tests/RecordingGraphics.h).
- Delete `PlaceTestImprovementsAroundBases_` and both `#ifdef AC_PLACE_TEST_IMPROVEMENTS` blocks
  in [Engine.cpp](src/game/Engine.cpp).
- Add `__pycache__/` to [.gitignore](.gitignore). Untrack
  `__pycache__/extract_terrain.cpython-312.pyc` and
  `tools/__pycache__/generate_ui_style.cpython-312.pyc`.

## 2. Rocky and rolling draw as full-tile cells (`delete-rockiness-inset`)

[smac-terrain-textures.md](../../docs/thinker/smac-terrain-textures.md) describes the rolling and
rocky cells as keyed overlays on the whole tile, corner-mapped like the other plain cells.

- First confirm against the running game. Take a side-by-side screenshot of a rocky area next to
  rolling and flat land, and record the result in the thinker doc.
- The Rockiness layer draws on the terrain shape like the other layers.
- Delete [TileSpriteEdgeInset.h](include/ui/TileSpriteEdgeInset.h),
  [TileSpriteEdgeInset.cpp](src/ui/TileSpriteEdgeInset.cpp) and their tests.
- In `TileRenderer.cpp`, delete `RockinessShape_`.
- Delete the `sprite_overlay_edge_inset_ratio` style key from the struct, the parser, the shipping
  style and the fixture style.

## 3. Missing-art policy (`missing-art-policy`)

Art comes from the player's SMAC install. When configured art fails to load:

- Terrain layers draw nothing. The tile's fill (`FillColor`) shows underneath.
- Objects draw the existing magenta and black checker.

Code and keys to remove:

- Delete `DrawProceduralMoisture_`, `DrawProceduralRockiness_`, `DrawProceduralRiver_`, the
  fungus fill, `ShouldSkipLandProceduralOverlays_`, `FillInsetShape_`, and `SubShape_` /
  `PointInShape_` once nothing uses them.
- Delete these style keys: `moist_center_color`, `wet_center_color`, `rolling_ring_color`,
  `rocky_ring_color`, `landform_ring_outer_inset_ratio`, `landform_ring_inner_inset_ratio`,
  `river_color`, `river_line_thickness_ratio`, `fungus_color`.

The requirement changes, so the `TileRendererTests` cases that assert procedural cues are replaced
by cases asserting the new policy: terrain draws only the fill, and objects draw the checker.

## 4. Lattice geometry in one place (`lattice-geometry`)

In [MapUtils.h](include/game/map/MapUtils.h):

- `enum class DiamondCorner_t { West, North, East, South }`.
- `k_CornerNeighbors`: the three lattice offsets that share each corner, in the order the current
  tables use. Neighbor `i` meets the point at its own corner `(corner + 1 + i) % 4`.
- `k_EdgeNeighbors`: N, E, S, W on screen (the NE, SE, SW, NW diamond edges).
  `ForEachOrthogonalNeighbor` uses it.
- `k_RingNeighbors`: the 8 neighbors clockwise from the N corner.
- One `LatticeOffset_t { int p; int q; }` for all three tables.

New header `ui/TileShapeGeometry.h`:

- `k_IsoHeightRatio`.
- `template<typename T> struct DiamondValues_t { T center, west, north, east, south; }`.
- `FlatTileShape` and `ShapeContains`, moved off `TileRenderer`.
- `SeatOf(const TileShape_t&)`: the mean of the four corners. `TryDrawOccupantPath_`,
  `DrawMissingArt_` and `MapViewport::FootprintOrigin` all use it.

Callers:

- `WaterShading`, `TileRelief`, `CoastOverlay`, `TileAutotile`, `WorldDisplay`'s grid edges and
  the road links use the MapUtils tables and delete their own `GridDelta_t` and local tables.
- The road links index `k_RingNeighbors` and keep today's cell numbering.
- `DiamondShades_t`, `TileLifts_t`, `TileShades_t` and `TileLevels_t` become `DiamondValues_t`
  instantiations. `CoastCorner_t` becomes `DiamondCorner_t`.
- `MapViewport` and `BaseWorkableAreaDisplay` include `TileShapeGeometry.h` instead of
  `TileRenderer.h`.

## 5. SpriteLibrary and a TileRenderer instance (`sprite-library`)

New `ui/SpriteLibrary.h`:

```cpp
// Loads each sprite path into the Graphics backend once. A path that fails to load is logged
// once and reported missing from then on.
class SpriteLibrary
{
public:
    using FileExists_t = std::function<bool(const std::string&)>;
    SpriteLibrary(Graphics& rGraphics, FileExists_t fileExists);
    bool Ensure(const std::string& path);
};
```

Ownership and construction:

- `Engine` constructs one `SpriteLibrary` over its `Graphics`, with `std::filesystem::exists`.
- `Engine` constructs one `TileRenderer(SpriteLibrary&, const TileRendererStyle_t&)` and passes
  `TileRenderer&` through `ViewFactory` to `WorldView` (→ `WorldDisplay`, `MinimapDisplay`,
  `LocationPanel`) and `BaseView` (→ `BaseWorkableAreaDisplay`).

`TileRenderer` changes:

- Its static functions become const members: `FillColor`, `RenderTerrain`, `RenderObjects`,
  `Render`.
- It reads its style from the constructor argument instead of the global `Style()`.
- `SpriteCache_` and `EnsureSpriteLoaded_` are deleted.
- The coast sprite paths (2 parts × 4 corners × 8 cases) are built once in the constructor.

Other changes:

- `FactionBaseArtCache` loads its textures through `SpriteLibrary` and drops its own texture
  state map. It keeps its colours cache.
- Tests construct `SpriteLibrary` over `RecordingGraphics` with a set of existing paths.
  `StubSprites.h` and every stub-PNG writer in the UI tests are deleted.

## 6. Occupant art blocks with a declared layer (`occupant-art`)

### Config

The six art keys (`sprite_paths`, `sprite_tiles`, `sprite_yield_rows`, `ground_sprites`,
`hides_sprites_of`, `sprite_overhang_ratio`) become one optional `art` object on each terrain and
improvement entry:

```json
{"id": "Rocky",   "art": {"layer": "rockiness", "variants": {"land": [".../rocky_0.png", ".../rocky_1.png"]}}}
{"id": "Moist",   "art": {"layer": "moisture", "tiles": {"layout": "blob", "land": ".../moist/{mask}.png"}}}
{"id": "Ocean",   "art": {"layer": "landform", "variants": {"sea": [".../ocean.png"]}, "depth_shade": {"offset": -16, "max": 18}}}
{"id": "Forest",  "art": {"layer": "vegetation", "tiles": {"layout": "edges", "land": ".../forest/{mask}.png"}, "fill_color": [34, 140, 56, 255]}}
{"id": "Road",    "art": {"layer": "road", "tiles": {"layout": "links", "land": ".../road/{mask}.png", "link_occupants": ["Road", "Base"]}}}
{"id": "Farm",    "art": {"layer": "object", "overhang": 0.24, "yield_rows": {"stat": "nutrients", "land": ["..."]}, "ground": {"Arid": ["..."], "Moist": ["..."], "Wet": ["..."]}}}
{"id": "Mine",    "art": {"layer": "object", "overhang": 0.24, "variants": {"land": [".../mine.png"]}}}
```

Keys:

- `layer`: `landform`, `moisture`, `rockiness`, `landmark`, `vegetation`, `river`, `road` or
  `object`. Required.
- `variants` / `tiles` / `yield_rows`: give exactly one. `yield_rows` is allowed only on
  `object`. The `links` layout is allowed only on `road`.
- `ground`: art drawn in place of the moisture layer, keyed by moisture name.
- `hides`: occupants whose object art is not drawn while this one is present.
- `overhang`: allowed only on `object`.
- `depth_shade`: allowed only on `landform`. It is the water shade range that
  `water_shading.shades` holds today.
- `fill_color`: overrides the elevation fill and the minimap colour. The last occupant in
  `ForEachOccupant` order that has one wins. This replaces the `forest_color` style key and the
  `k_Forest` / `k_Fungus` checks in `FillColor`.

Entries that draw nothing (`Flat`, `Water`, `Aquifer`, landmarks without art, `Base`) have no
`art`.

### Parsing and validation

- `ImprovementConfig_t` holds `std::optional<OccupantArt_t> art` in place of the six fields.
- `OccupantArt_t` lives in `game/map/OccupantArt.h`. Its parser is in
  `game/map/OccupantArtParser.cpp`, called from `ParseImprovementBody`.
- A `tiles` pattern is expanded at parse into one path per mask.
- `ExpandFeatureTagReferences` keeps validating `hides`, `link_occupants` and
  `replaces_links_of`.
- `water_shading.shades` leaves the UI style. `deep_landform`, `shelf_landform`,
  `deep_from_shade`, `coast_shades`, `depth_shades` and `detail_meters` stay there.
- After both the style and the occupant registry load, the composition root calls
  `ValidateTerrainArtReferences(style, registry)`. It throws unless `deep_landform`,
  `shelf_landform` and `coast_shades` each name a landform that has a `depth_shade`.

### Rendering

`RenderTerrain` walks `Tile::ForEachOccupant` and draws occupants grouped by layer in this order:

1. landform, moisture (or an improvement's `ground`), rockiness, landmark, vegetation
2. the coast
3. river, road
4. fog haze

`RenderObjects` draws `object` art: terrain occupants first, then improvements, skipping those
named by a present occupant's `hides`. Layer behavior stays in C++, keyed by layer:

- moisture: the "water or at least as wet" neighbor rule
- landform on water: the deep/shelf swap and the depth shades
- road: links

Every other tile set matches neighbors that have the same occupant.

Delete:

- [TileLayer.h](include/game/map/TileLayer.h), `TileLayerResolver.h` / `.cpp` and their
  `MapRulesTests` cases
- `ContentIdToConfigId_`, `FindLayerOccupant_`, `FindOccupantByConfigId_`
- the hardcoded id list in `RenderObjects`
- the `forest_color` and `water_shading.shades` style keys

## 7. Relief cache (`relief-cache`)

`MapViewport` keeps one `DiamondValues_t<float>` of lifts and one of shades per tile, indexed by
`WorldMap::GetTileIndex`. The cache is rebuilt when any of these changes:

- `WorldMap::GetAppearanceRevision()`
- the relief mode
- the relief style

`ShapeAt_`, `PixelOriginOf` and `MaxLiftPixels_` read the cache. `ResolveTileLifts` and
`ResolveTileShades` are called only while rebuilding it.

## 8. Text measurement (`measure-text`)

- Add `virtual float MeasureTextWidth(const std::string& text, unsigned int size) const` to
  `Graphics`. SFML measures with `sf::Text::getLocalBounds`. `NullGraphics` and
  `RecordingGraphics` return `text.size() * size * 0.5f`.
- `WorldDisplay::RenderBases_` and `BaseWorkableAreaDisplay`'s centred text use it.
- Delete `base_name_char_width_ratio` and `tile_text_char_width_ratio`.

## 9. Fogged tiles draw remembered occupants (`tile-memory`)

### Model

New `game/faction/FactionTileMemory.h`, owned by `Faction` next to the explored map:

- Per tile, it stores the terrain and improvement occupant lists the faction last saw.
- `Record(const Tile&)` copies the tile's current lists.
- `Occupants(const Tile&)` returns `TileOccupants_t { span terrain; span improvements; }`.
- It has a `Revision`.

`Record` is called:

- at the end of `FactionVisibleMap::RebuildFromSources` for every visible tile
- in `VisibilityRules` where the explored map is `MarkAll`ed
- in `DiplomaticProposalEffects` after `MergeFrom`

For the last two, add
`// TODO: confirm in terranx.exe what a faction sees on tiles explored by map trade or shroud removal`.

### What a viewer sees

New `ui/world/MapAppearance.h`:

```cpp
// The occupants a viewer sees: live on tiles in sight, remembered elsewhere.
class MapAppearance
{
public:
    MapAppearance(const WorldMap& rMap, const FactionVisibleMap* pVisible,
                  const FactionTileMemory* pMemory);
    const WorldMap& Map() const;
    TileOccupants_t OccupantsOf(const Tile& rTile) const;
};
```

Null visible map or memory means live everywhere.

### Renderer changes

- `TileRenderer::RenderTerrain` / `RenderObjects` / `FillColor` take `const MapAppearance&` in
  place of `const WorldMap*`. Occupant walks and tile-set neighbor rules read
  `OccupantsOf`.
- The coast, water shading and relief keep reading live elevation and surface. Add
  `// TODO: confirm in terranx.exe whether remembered tiles keep their old elevation`.
- The yield lookup for yield rows stays live. Add
  `// TODO: confirm which yield SMAC uses for remembered farm structures`.
- `WorldDisplay`, `MinimapDisplay` (its cache key adds the memory revision), `LocationPanel` and
  `BaseWorkableAreaDisplay` build the player's appearance.
- The `pMap == nullptr` rendering path is deleted.

### Tests

Using fixtures:

- An improvement added while a tile is out of sight does not draw until the tile is seen again.
- An improvement removed out of sight keeps drawing.
- Tiles in sight draw live occupants.

## 10. Docs (`docs`)

- [graphics-system.md](../../docs/architecture/graphics-system.md) and
  [ui-system.md](../../docs/architecture/ui-system.md):
  - `SpriteLibrary`, the `TileRenderer` instance and its injection
  - art blocks and layer dispatch
  - the missing-art policy
  - `TileShapeGeometry`, the relief cache, `MeasureTextWidth`, `MapAppearance`
  - delete the edge-inset and procedural-cue sections
- [map-system.md](../../docs/architecture/map-system.md):
  - `OccupantArt_t` on `ImprovementConfig_t`
  - the corner/edge/ring tables in `MapUtils`
  - `FactionTileMemory`
  - delete `TileLayerResolver`
- [graphics_assets_roadmap_04a90a53.plan.md](graphics_assets_roadmap_04a90a53.plan.md): the
  config hook is the `art` block, and later renderers load through `SpriteLibrary`.
