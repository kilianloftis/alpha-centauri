# Map System Architecture

```mermaid
graph TB
    subgraph "Map System"
        TileMap[TileMap]
        Tile[Tile]
    end

    subgraph "Tile Identity"
        Position[Position<br/>x,y int]
    end

    subgraph "Tile Characteristics"
        Moisture[Moisture<br/>enum: Arid/Moist/Wet]
        Rockiness[Rockiness<br/>enum: Flat/Rolling/Rocky]
        Elevation[Elevation<br/>int -4000 to 4000 meters]
    end

    subgraph "Tile Features"
        River[River<br/>bool]
        Fungus[Fungus<br/>bool]
        Landmark[Landmark<br/>string]
        Improvements[Improvements<br/>vector<string>]
        Bonus[Bonus<br/>string]
        WorkerAssigned[WorkerAssigned<br/>baseId int, -1=unworked]
    end

    subgraph "Effects (config/improvements.json)"
        ImprovementRegistry[ImprovementRegistry]
        CollectTileEffects[CollectTileEffects]
        ResolveTileYield[ResolveTileYield]
        ResolveTileDefenseMultiplier[ResolveTileDefenseMultiplier]
    end

    subgraph "Game Integration"
        Base[Base]
        Faction[Faction]
    end

    TileMap --> Tile
    Tile --> Position
    Tile --> Moisture
    Tile --> Rockiness
    Tile --> Elevation
    Tile --> River
    Tile --> Fungus
    Tile --> Landmark
    Tile --> Improvements
    Tile --> Bonus
    Tile --> WorkerAssigned
    Tile --> CollectTileEffects
    CollectTileEffects --> ImprovementRegistry
    CollectTileEffects --> ResolveTileYield
    CollectTileEffects --> ResolveTileDefenseMultiplier
    Base --> Tile
    Faction --> Base

    style Tile fill:#f9f,stroke:#333,stroke-width:4px
    style TileMap fill:#fbf,stroke:#333,stroke-width:3px
    style Position fill:#bbf,stroke:#333,stroke-width:2px
    style WorkerAssigned fill:#bbf,stroke:#333,stroke-width:2px
    style ImprovementRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style CollectTileEffects fill:#bfb,stroke:#333,stroke-width:3px
```

## Component Overview

### Tile
- **Purpose**: Represents a single tile on the game map with all its characteristics. `Tile` itself stays a plain data holder — it has no knowledge of the effects system or `ImprovementRegistry`; all effects-based resolution (yield, defense) lives in free functions that take a `Tile` and a registry, the same pattern used elsewhere in the effects system (e.g. `CollectFromBuildings` takes a `Faction` and a `BuildingRegistry`).
- **Responsibilities**:
  - Store terrain characteristics:
    - Moisture (enum: Arid, Moist, Wet)
    - Rockiness (enum: Flat, Rolling, Rocky)
    - Elevation (int: -4000 to 4000 meters)
  - Track tile features (Rivers, Fungus, Landmarks, Improvements)
  - Expose `GetFeatureIds()` so the effects system can resolve yield/defense from terrain and improvements through one mechanism — see Tile Improvement Effects below
- **Composition**:
  - `Position`: x,y coordinates on the map grid
  - `Moisture`: Enum (Arid, Moist, Wet) - affects nutrient production via its `Moist`/`Wet` entry in `config/improvements.json`
  - `Rockiness`: Enum (Flat, Rolling, Rocky) - affects mineral production and (for Rocky) grants a defense bonus, via its entry in `config/improvements.json`
  - `Elevation`: Integer in meters, range -4000 to 4000 - the raw seed for energy production (`GetElevationEnergySeed()`); River/improvement bonuses layer on top via effects
  - `River`: Boolean flag for river presence (grants an energy bonus via its improvements.json entry)
  - `Fungus`: Boolean flag for alien fungus presence (grants a defense bonus via its improvements.json entry). Presence-only for now — spreading fungus turn-over-turn is a separate future enhancement, not implemented.
  - `Landmark`: Optional landmark identifier (e.g., "Monsoon Jungle", "Fungal Tower")
  - `Improvements`: Vector of built improvements (e.g., "Farm", "Mine", "Bunker") — also gains `"Base"` automatically when a `BaseManager` is founded on the tile
  - `Bonus`: Optional tile bonus ID (e.g., "nutrient_rich_soil", "geothermal_vent") — a separate, not-yet-wired-up system, see Tile Bonus System below
  - `WorkerAssigned`: Integer base ID tracking which base has a worker on this tile (-1 if unworked; one worker per tile limit)
  - `bWorked`: Mutable boolean flag set when a `Pop` is actively assigned to this tile; `Pop` clears it on destruction through its `const Tile*`
- **Worker Assignment**:
  - `AssignWorker(baseId)`: Marks tile as worked by the given base
  - `UnassignWorker()`: Removes worker assignment (sets baseId to -1)
  - `IsWorkerAssigned()`: Check if tile already has a worker (enforces one worker per tile rule)
  - `GetWorkedByBaseId()`: Returns the base ID currently working this tile, or -1 if unworked
  - `SetWorked(bWorked)`: Sets the worked flag directly (used by `Pop` assignment lifecycle)
  - `IsWorked()`: Returns the worked flag

### Tile Visual Layer System

```mermaid
graph TB
    subgraph "Tile Visual Layer System"
        Resolver[TileLayerResolver]
        Layers[std::array&lt;TileLayer_t&gt;]
        Landform[Landform<br/>water / flat / rolling]
        Moisture[Moisture<br/>arid / moist / wet]
        Rockiness[Rockiness<br/>rocky / empty]
        Vegetation[Vegetation<br/>farm / forest / empty]
        Road[Road<br/>road / empty]
        Improvement[Improvement<br/>dominant other / empty]
    end

    Tile[Tile] --> Resolver
    Resolver --> Layers
    Layers --> Landform
    Layers --> Moisture
    Layers --> Rockiness
    Layers --> Vegetation
    Layers --> Road
    Layers --> Improvement

    style Resolver fill:#fbf,stroke:#333,stroke-width:3px
    style Layers fill:#f9f,stroke:#333,stroke-width:3px
```

- **Purpose**: Provides an ordered, render-only representation of a tile's visual contents
- **Components**:
  - `TileLayerType_t`: Enum defining the visual layer order (Landform, Moisture, Rockiness, Vegetation, Road, Improvement)
  - `TileLayer_t`: Pair of layer type and optional content ID string (`std::optional<std::string>`)
  - `ResolveTileLayers(const Tile&)`: Free function that maps a `Tile`'s gameplay data to the layer array
- **Rationale**: Separates tile gameplay data from rendering data, so changes to visuals do not affect resource calculation or other systems
- **Layer Order** (bottom to top):
  1. `Landform`: water, flat, or rolling
  2. `Moisture`: arid, moist, or wet
  3. `Rockiness`: rocky overlay (empty if not rocky)
  4. `Vegetation`: farm or forest
  5. `Road`: road
  6. `Improvement`: dominant non-vegetation, non-road improvement (e.g., Borehole, Monolith)
- **Open Questions / TODOs**:
  - Water threshold and landform generation rules
  - Vegetation mutual exclusivity and placement rules (Borehole/Base vs Farm/Forest)
  - Improvement rendering priority and monolith/landmark handling

### Tile Improvement Effects
- **Purpose**: Unifies terrain classification, natural features, landmarks, player-built improvements, and a founded base behind one config type (`ImprovementConfig_t`) and one lookup (`Tile::GetFeatureIds()` → `ImprovementRegistry::Find(id)`), since all of them answer the same two questions: what effects do they grant, and what do they exclude. Full details (scope semantics, the `ThisTile` resolution pattern, the seeded-energy pattern) are in `docs/architecture/effects-system.md`'s "Tile Improvement Effects" section — this is the map-system-facing summary.
- **Components**:
  - `ImprovementConfig_t` / `ImprovementConfigParser` / `ImprovementRegistry` (`include/game/map/ImprovementConfigParser.h`, `ImprovementRegistry.h`) — id, name, mineral cost, required tech, `excludes` (incompatible feature ids), and an `effects` array.
  - `CollectTileEffects`, `ResolveTileYield`, `ResolveTileDefenseMultiplier` (`include/lib/effects/ActiveEffect.h`) — gather and resolve a tile's own effects.
  - `CanBuildImprovement(tile, candidate)` (`include/game/map/ImprovementConfigParser.h`) — exclusivity check (e.g. Farm excludes Rocky). Not wired into any UI yet; there's no improvement-construction flow to call it from.
- **Configuration**: `config/improvements.json` — one array covering terrain values (`Flat`/`Rolling`/`Rocky`, `Arid`/`Moist`/`Wet`), natural features (`River`, `Fungus`), and improvements (`Farm`, `Mine`, `Bunker`, `Base`).
- **Combat bonus example**: `Rocky`, `Fungus`, and `Bunker` each grant a `StatModifier` effect on `StatId::Defense` with `op: MultiplyArithmetic, amount: 1.25` (+25%, stacking additively per `ResolveStatModifiers`'s arithmetic-factor formula). `Base` grants a larger placeholder bonus the same way. No combat system exists yet to consume `ResolveTileDefenseMultiplier` — it's exposed as a ready-to-call resolver.
- **Aura example**: `Sensor` (`radius: 2`) projects its `+25%` defense bonus to every tile within 2 tiles (Manhattan), not just its own — `ResolveTileDefenseMultiplier` takes a `WorldMap` specifically to scan for these. This is the one tile resolver that needs map access; `ResolveTileYield`/`CollectTileEffects` only ever look at a tile's own feature ids.

### Tile Bonus System
- **Purpose**: Defines special resource bonuses that can be applied to individual tiles
- **Components**:
  - `TileBonusConfig`: Data structure holding bonus definition (id, name, description, nutrient/mineral/energy bonuses, frequency, sprite path)
  - `TileBonusConfigParser`: Parses tile bonus definitions from JSON config
  - `TileBonusRegistry`: Loads and provides access to all tile bonuses at runtime
- **Configuration**: `config/tile_bonuses.json` - defines available bonuses and their effects
- **Example Bonuses**:
  - `nutrient_rich_soil`: +2 nutrients, frequency 100 (common)
  - `mineral_deposit`: +2 minerals, frequency 100 (common)
  - `geothermal_vent`: +2 energy, frequency 50 (uncommon)
  - `bounty`: +2 to all resources, frequency 10 (rare)
  - `rare_earth_deposit`: +2 nutrients, +2 minerals, frequency 30 (uncommon)
- **Frequency System**: Higher values = more common during map generation. Used by world generator to weight bonus placement probability.

### TileMap (Future)
- **Purpose**: Container managing all tiles on the game map
- **Responsibilities**:
  - Store 2D grid of Tile instances
  - Provide spatial queries and tile access
  - Handle map generation and persistence
- **Rationale**: Will be implemented when world generation and map persistence are added

## Integration with Game Systems

### Base System
- Bases work tiles to extract resources
- `Base::SetPosition(x, y)` / `Base::GetX()` / `Base::GetY()` track map position
- `Base::GetWorkableTilePositions()` returns `const Tile*` pointers for all tiles in a 5×5 grid with the four corners removed (Manhattan distance ≤ 3 within `[-2,2]` offsets), excluding the base's own tile — 20 tiles total. Enemy-unit blocking is a TODO pending unit implementation.
- `Base::SetWorkedTiles()` aggregates resources from worked tiles
- `TileResources_t` struct used to pass resource totals
- Each worker assigned to a tile contributes that tile's resource production
- `Tile::AssignWorker(baseId)` records which base is working the tile; prevents double-assignment by checking `IsWorkerAssigned()` before assigning
- `BaseManager`'s constructor adds `"Base"` to its tile's improvements, so the base's own garrison defense bonus flows through the same `ImprovementRegistry` lookup as Bunker/Rocky/Fungus (see Tile Improvement Effects above). This is also why `BaseManager` holds a non-const `Tile&` rather than `const Tile&`.

### Faction Economy
- Faction's economy aggregates resources from all bases
- Energy from tiles feeds into faction energy budget
- Minerals from tiles fund production
- Nutrients drive population growth

## Design Rationale

### Separation of Concerns
- Tile stores only its own state and calculates its own production
- Bases manage which tiles are worked
- Economy aggregates at faction level
- Follows Single Responsibility Principle

### Extensibility
- Improvements system allows extending tile functionality without modifying Tile class
- Landmark system supports special terrain features
- Resource and defense calculation are entirely effects-driven via `config/improvements.json` — adding a new improvement, or changing what Rocky/Fungus grant, never touches `Tile` or its consumers' C++

### Moddability
- Terrain characteristics use semantically meaningful types (enums for moisture/rockiness, actual meters for elevation) for world-gen and rendering, but resolve through the same string-id effects lookup as improvements for yield/defense purposes
- Improvements and Landmarks use string IDs for easy content addition
- Resource and defense formulas live in `config/improvements.json`, not in code

## Future Enhancements

- **TileMap**: 2D grid container with spatial queries
- **World Generation**: Procedural map generation creating tiles with varied terrain
- **Terraforming**: Ability to modify tile characteristics (moisture, elevation)
- **Fungal Spread**: `Tile::GetHasFungus()` is presence-only today (manually settable, never placed by `WorldGenerator`); having fungus actually spread turn-over-turn is still future work
- **Weather System**: Dynamic moisture/river modifications based on climate
- **Improvement construction**: no UI/production flow lets a player actually build Farm/Mine/Bunker yet — `Tile::AddImprovement()` and `CanBuildImprovement()` exist but are unconsumed outside the automatic `"Base"` improvement
- **Combat system**: `ResolveTileDefenseMultiplier()` is ready to call but nothing resolves attacks yet
