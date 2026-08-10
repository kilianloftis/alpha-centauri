# UI System Architecture

```mermaid
graph TB
    subgraph "Core Abstractions"
        UIManager[UIManager<br/>(abstract base class)]
        IGameView[IGameView<br/>(interface)]
        IWorldView[IWorldView<br/>extends IGameView<br/>what UIManager needs of the map view]
        UIElement[UIElement<br/>(abstract base class)]
        UIWorldMap[UIWorldMap<br/>extends UIElement]
        UIPanel[UIPanel<br/>extends UIElement]
        UIPopup[UIPopup<br/>extends UIElement]
    end

    subgraph "UIManager Implementation"
        UIManagerImpl[UIManagerImpl<br/>owns view stack]
        Factory[CreateUIManager()]
    end

    subgraph "Views"
        WorldView[WorldView<br/>implements IWorldView]
        BaseView[BaseView<br/>implements IGameView<br/>coordinates panels]
        ResearchView[ResearchView<br/>implements IGameView]
        WorldMapElement[WorldMapElement<br/>implements UIWorldMap]
        InfoPanelElement[InfoPanelElement<br/>implements UIPanel]
    end

    subgraph "View Factory"
        ViewFactory[ViewFactory]
    end

    subgraph "Base Panels"
        IBasePanel[IBasePanel<br/>interface]
        BaseDisplay[BaseDisplay<br/>implements IBasePanel]
        BaseWorkableAreaDisplay[BaseWorkableAreaDisplay<br/>implements IBasePanel]
        PopulationDisplay[PopulationDisplay<br/>implements IBasePanel]
        GrowthDisplay[GrowthDisplay<br/>implements IBasePanel]
    end

    subgraph "Base Popups"
        ListSelectorPopup[ListSelectorPopup<br/>extends UIElement<br/>the one modal list picker]
    end

    subgraph "Dependencies"
        Graphics[Graphics]
        Input[Input]
        Engine[Engine]
    end

    Engine -->|owns| UIManager
    Engine -->|owns| ViewFactory
    Engine -->|sets world view on| UIManager
    ViewFactory -->|creates| WorldView
    ViewFactory -->|creates| BaseView
    ViewFactory -->|creates| ResearchView
    ViewFactory -->|creates| UnitDesignerView

    BaseView -->|owns| BaseDisplay
    BaseView -->|owns| PopulationDisplay
    BaseView -->|owns| GrowthDisplay
    BaseView -->|refs| BaseWorkableAreaDisplay
    BaseView -->|coordinates via| IBasePanel
    BaseDisplay -.->|implements| IBasePanel
    BaseWorkableAreaDisplay -.->|implements| IBasePanel
    PopulationDisplay -.->|implements| IBasePanel
    GrowthDisplay -.->|implements| IBasePanel

    UIManager -->|manages stack of| IGameView
    UIManagerImpl -->|holds persistent| IWorldView
    IWorldView -.->|extends| IGameView
    WorldView -.->|implements| IWorldView
    UIManagerImpl -.->|implements| UIManager
    Factory -->|returns| UIManagerImpl

    IGameView -->|owns| UIElement

    UIWorldMap --> UIElement
    UIPanel --> UIElement
    UIPopup --> UIElement

    UIManagerImpl --> Graphics
    UIManagerImpl --> Input

    style UIManager fill:#bbf,stroke:#333,stroke-width:4px
    style IGameView fill:#bbf,stroke:#333,stroke-width:4px
    style IWorldView fill:#bbf,stroke:#333,stroke-width:2px
    style UIElement fill:#bbf,stroke:#333,stroke-width:2px
    style UIManagerImpl fill:#bfb,stroke:#333,stroke-width:2px
    style WorldView fill:#bfb,stroke:#333,stroke-width:2px
    style BaseView fill:#bfb,stroke:#333,stroke-width:2px
    style ResearchView fill:#bfb,stroke:#333,stroke-width:2px
    style Factory fill:#ff9,stroke:#333,stroke-width:2px
    style ViewFactory fill:#ff9,stroke:#333,stroke-width:2px
    style IBasePanel fill:#bbf,stroke:#333,stroke-width:2px
    style BaseDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style BaseWorkableAreaDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style PopulationDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style GrowthDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style ListSelectorPopup fill:#bfb,stroke:#333,stroke-width:3px

    subgraph "Unit Designer"
        UnitDesignerView[UnitDesignerView<br/>implements IGameView]
        DesignStatsDisplay[DesignStatsDisplay<br/>extends UIElement<br/>shows combined stats]
        DesignListPanel[DesignListPanel<br/>extends UIElement<br/>shows saved designs]
        UnitStatusPanel[UnitStatusPanel<br/>extends UIElement<br/>shows active/in-prod counts]
        UnitDesignerState[UnitDesignerState_t<br/>draft component selection]
    end
    UnitDesignerView -->|owns| DesignStatsDisplay
    UnitDesignerView -->|owns| DesignListPanel
    UnitDesignerView -->|owns| UnitStatusPanel
    UnitDesignerView -->|spawns on click| ListSelectorPopup
    UnitDesignerView -->|holds| UnitDesignerState
    DesignStatsDisplay -->|reads| UnitDesignerState
    UnitDesignerView -->|saves to| Military
    DesignListPanel -->|reads| Military

    style UnitDesignerView fill:#bfb,stroke:#333,stroke-width:2px
    style DesignStatsDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style DesignListPanel fill:#bfb,stroke:#333,stroke-width:2px
    style UnitStatusPanel fill:#bfb,stroke:#333,stroke-width:2px
    style UnitDesignerState fill:#ffd,stroke:#333,stroke-width:2px

    subgraph "WorldDisplay"
        WorldDisplayNode[WorldDisplay]
        TileLayer[Tile Layer]
        BaseLayer[Base Layer]
        UnitLayer[Unit Layer]
    end
    WorldMapElement -->|renders via| WorldDisplayNode
    WorldDisplayNode --> TileLayer
    WorldDisplayNode --> BaseLayer
    WorldDisplayNode --> UnitLayer

    style WorldDisplayNode fill:#f9f,stroke:#333,stroke-width:3px
    style TileLayer fill:#bbf,stroke:#333,stroke-width:2px
    style BaseLayer fill:#bbf,stroke:#333,stroke-width:2px
    style UnitLayer fill:#bbf,stroke:#333,stroke-width:2px
```

## Component Overview

### UIElement (Abstract Base Class)
- **Purpose**: Base class for all UI elements
- **Properties**: position (x, y), size (width, height), visibility
- **Virtual Methods**:
  - `Draw(Graphics&)`: Render the element
  - `Update(`: Update element state
  - `HandleKey(KeyEvent_t&)`: Handle key input; returns true to consume
  - `HandleMouse(MouseEvent_t&)`: Handle mouse input; returns true to consume
- **Helper**: `Contains(x, y)`: True if the point is within element bounds

### IGameView (Interface)
- **Purpose**: A screen or layer in the view stack, owned by `UIManager`. Manages its own `UIElement`s.
- **Lifecycle**: `OnPushed()` / `OnPopped()` hooks for setup and teardown
- **Virtual Methods**:
  - `Render(Graphics&)`: Draw the view and its elements
  - `Update(float)`: Tick the view
  - `HandleKey(KeyEvent_t&)`: Handle a key event
  - `HandleMouse(MouseEvent_t&)`: Handle a mouse event (fallback after element routing)
  - `GetElements()`: Return owned `UIElement*` list for input hit-testing

### IWorldView (Interface)
- **Purpose**: What `UIManager` needs from the persistent map view, beyond `IGameView`:
  `UpdateCameraInput(bool bEnabled, optional<MousePosition_t>)` and
  `ProcessPendingAutoEndTurn()`. `BlocksTurnAdvance` is not here — `IGameView` already declares
  it with a default.
- **Why**: `UIManager` named `WorldView` concretely, so anything linking the manager also linked
  `GameState`, every registry, and the map renderer. The manager's own rules (push/pop, when a
  closed view stops receiving input, the turn gate) were untestable as a result.
- **Implementer**: `WorldView`. Tests substitute a fake.

### UIManager (Abstract Base Class)
- **Purpose**: Owned by `Engine`. Manages the view stack, routes input, triggers rendering.
- **Virtual Methods**:
  - `Initialize(Graphics&, Input&)`: Store backends
  - `Update(float)`: Tick the top view
  - `ProcessInput()`: Drain key/mouse queues and route to top view / elements
  - `Render()`: Clear, render all stacked views, display
  - `PushView(unique_ptr<IGameView>)`: Push a view onto the stack
  - `PopView()`: Pop the top view
  - `ShouldExit()` / `RequestExit()`: Exit-flag management

### Input Routing (ProcessInput)
1. Key events → `topView.HandleKey()`
2. Mouse button events → find first `UIElement` under cursor via `Contains()` → `element.HandleMouse()`; if not consumed, fallback to `topView.HandleMouse()`

Closed views are pruned **per event**, not per frame and not per drain loop: one keystroke can
close the top view, and the next event in the same batch must reach whatever is active now.
Pruning used to happen inside `Render`, so a view closed by a keystroke kept receiving the whole
subsequent mouse batch.

Camera input (`UpdateCameraInput`) is driven from `Update`, not `Render`, so a second render pass
in one tick cannot apply edge scrolling twice. Edge scrolling is disabled while an overlay covers
the map.

### Render Order
Views are rendered bottom-to-top through the stack. Each view renders its own `UIElement`s in its `Render()` method.

### WorldDisplay Viewport
- **Purpose**: Controls which portion of the world map is visible on screen.
- **State**: `m_tileSize` (pixel size per tile), `m_cameraX`/`m_cameraY` (top-left tile coordinates of the viewport).
- **Rendering**: `Render(x, y, w, h)` computes how many tiles fit in the given pixel area from `m_tileSize`, then renders only the tile range `[cameraX, cameraX + tilesWide) x [cameraY, cameraY + tilesHigh)`. Layers are drawn in this order: tiles, bases, then units.
- **Configurability**: `SetTileSize()` and `MapViewport::SetCamera()` are the sole control points. Tile size is intentionally configurable to support zoom and per-platform tuning.
- **Mouse hit-testing**: `WorldView::HandleMouse` reads the viewport state from `WorldDisplay` and translates screen-relative tile indices back to world tile coordinates by adding the camera offset.
- **Unit Layer**: Unit markers are rendered on top of bases by querying `WorldMap::GetUnitsOnTile()` for each visible tile. Multiple units on the same tile are drawn side-by-side; faction coloring is a future TODO.
- **Unit Selection**: Left-clicking a tile with a base opens that base (`m_onOpenBase`), even when units are garrisoned there — the base screen's unit stack is how the player picks a unit on a base tile. Otherwise, left-clicking a tile with units selects the first visible unit on that tile (`WorldView::SelectUnitAtTile_`). The selected unit is highlighted with a yellow border and is passed to `WorldDisplay` via `SetSelectedUnit()`.
- **Unit Orders**: With a selected unit, the `H` key issues a `HoldOrder_t` via `UnitOrderInputController`. Order execution is delegated to the turn-processing `UnitOrderExecutor`.
- **Move Orders**: Right-clicking and holding for one second, then releasing, assigns a `MoveOrder_t` to the selected unit for the tile under the cursor on release. Short right-clicks are ignored. `MouseEvent_t::bPressed` is used to distinguish press and release events.

### WorldView Input Routing
- **Coordinator**: `WorldView::HandleKey` and `WorldView::HandleMouse` are thin coordinators that dispatch to owned sub-controllers before handling view-lifecycle input themselves.
- **CameraInputController**: Owned by `WorldView`; constructed with `WorldDisplay&` and `WorldMap&`. Handles arrow-key camera panning and will later own mouse edge-scroll state (last mouse position, scroll accumulator).
- **UnitOrderInputController**: Owned by `WorldView`; constructed with no dependencies. Dispatches hotkeys to unit orders through a `Key_t` → `std::function<void(Unit&)>` table (`H` → `HoldOrder_t`). Also handles right-click-and-hold for `MoveOrder_t`.
- **Dispatch order**: `HandleKey` tries the order controller, then the camera controller, then handles `Escape` and `Enter` directly. `HandleMouse` tries the order controller, then the camera controller, then handles left-click unit selection and base opening.

### ViewFactory
- **Purpose**: Creates `IGameView` instances from game state and graphics context
- **Dependencies**: `GameState`, `GameDataContext`, `Graphics`
- **Owner**: `Engine` creates and owns it during initialization
- **Methods**: `CreateWorldView`, `CreateBaseView`, `CreateResearchView`,
  `CreateSocialEngineeringView`, `CreateUnitDesignerView`, `CreateSettingsView`,
  `CreateCommlinksView`, `CreateCouncilVoteView`, `CreateSatelliteView`, `CreateCombatView`
- **Missing player faction**: `RequirePlayerFaction_()` throws. Every `Create*View` that needs the
  player faction reaches it from a player action, and a session without a player faction is
  broken, not a UI state — the previous `return nullptr` was dereferenced by `PushView`, which now
  also rejects a null view.

### Rendering from snapshots
Panels paint from precomputed state rather than re-deriving it per frame. Three places do this,
each with an explicit key:

- **`BaseDisplaySnapshot_t`** (`include/ui/base/BaseDisplaySnapshot.h`) — nutrient/mineral
  production, nutrients required, mineral cost, and the yield and work state of every workable
  tile. `BaseView` owns one and rebuilds it only when `ReadBaseDisplayKey` moves: the faction's
  effects version, the world `WorkedTileIndex` revision, the base's population revision, its
  home-unit revision, and the current production item. Previously `GrowthDisplay`,
  `ProductionDisplay` and `BaseWorkableAreaDisplay` between them drove two full
  `ResourceManager::ComputeWorked_` passes and twenty per-tile yield resolutions on **every
  paint**. Cheap reads (stockpiles, base name, population size) stay live and are deliberately
  not in the snapshot.
  Tile state is absent from the key because it cannot change while the view is open:
  terraforming resolves on turn advance, and `UIManager` refuses to advance the turn while an
  overlay covers the map.
- **`CouncilVoteWeightCache`** (`include/ui/council/CouncilVoteWeightCache.h`) — one entry per
  faction, keyed on the council revision, the faction's local effects version and its
  population. `PlanetaryCouncil::ComputeVoteWeight` copies the whole effect pool and resolves
  `CouncilVotes` modifiers; both council panels called it per member per paint.
- **`SatelliteSummaryPanel`** — the orbital-type list, faction list and census map are members
  filled by `Refresh()`, called from the constructor. Nothing can move the census while Summary
  mode is showing: an attack is reachable only from OrbitalAttack mode, and returning to Summary
  builds a new panel.

`SatelliteView` also stopped calling `Rebuild_()` on every selection: `SatelliteButtonListPanel`
now applies selection to its own buttons (`SetSelected`) and replaces its contents
(`SetItems`), so a click no longer destroys and recreates the tabs, the Attack button and both
lists — including, mid-callback, the button that was clicked. `Rebuild_()` remains for a mode
change, which genuinely replaces the layout.

### UiStyle — the theme, and its known shape problem
All chrome (colours, font-size ratios, sub-layouts) comes from `config/ui/style.json`, parsed into
`UiStyle` and read through the free function `Style()`. `Load` fails loudly on a missing file or
key and commits into the global only after a full parse; `Get` throws if used before `Load`.

**`UiStyle` is a process-global god object, and that is a known open finding.** It is a ~36-member
typed bag plus a file-scope singleton, so every new panel needs a nested struct, a `UiStyle`
member, a parser clone and another line in `Load` — open/closed growth — and tests cannot inject a
theme without mutating process state. The fix is per-feature style types owned and passed down
from the composition root, with `Style()` demoted to a bridge. Not done: it is ~290 call sites
across 61 files. What *has* been closed is the reason it stayed frightening — the UI now has
tests (`ac-ui`, `ViewFixture`, `RecordingGraphics`), so the conversion can be verified rather than
eyeballed.

Two smaller shapes are fixed: the growth and production panels share one
`ResourceLinesPanelStyle_t` (two instances of one type) instead of two identical structs that
could desync, and `ParseColor_` rejects an over-long array instead of silently dropping the
extras.

### Build target: `ac-ui`
Every view, panel and input controller lives in the `ac-ui` static library, which links `ac-core`
and depends only on the abstract `Graphics` / `Input` interfaces — never on a rendering backend.
The `alpha-centauri` executable is `main.cpp` plus `Engine.cpp` plus the SFML backend. The
layering the diagrams describe is therefore enforced by the build, and the test suite links
`ac-ui` to drive real views against `actest::RecordingGraphics`, which records draw positions
and colours.

### Factory: CreateUIManager()
Returns `UIManagerImpl`, a platform-agnostic implementation (no compile-time flag needed).

### ListSelectorPopup — the one list picker

Every "pick one of these" modal is a `ListSelectorPopup`: production, pop types, probe actions,
supply-crawl resources, unit components, council proposals, council ballots. It takes a title, an
empty-list message, a vector of row labels, and `onSelected(size_t index)`; the caller keeps its
own payload vector and indexes into it.

Seven near-identical copies preceded it and had already diverged — outside-click dismiss existed
in one, null-checking in another, and none clipped a long list. Consolidating means those rules
have one owner:

- **Bounded rows.** Only as many rows as fit below the header are laid out. When the list is
  longer, the bottom line is reserved for an `[a-b of N]` indicator, so it never paints over a row
  that would still be clickable underneath it. Arrow keys scroll.
- **Outside click dismisses**, which is reachable because modal routing delivers every press to
  the top modal (see below).
- **An absent handler throws at construction.** A selector whose click does nothing is a
  programmer error, not a state to render.
- **Style is a constructor parameter**, not a lookup, so a screen can keep its own colours and
  metrics (the unit designer's picker does) without a second widget or a second style type.

### Modal / overlay contract and turn gating

Turn processing uses `TurnProcessor::Advance` (Package 1 Yield/resume). The UI decides when
it is safe to call `Advance`:

- **Overlay** = entry on `UIManager`'s overlay stack (BaseView, Research, Council vote, Combat, …).
- **In-view modal** = a `UIElement` with `IsModal() == true` while open (selectors, proposals,
  ballots, probe/supply pickers, orbital dialogs, …).
- **Input:** While the active view has a top modal, every mouse press (including outside the
  chrome) and every key goes exclusively to that element. Global view shortcuts are suppressed
  while any overlay is active. Opening a second modal of the same kind dismisses the prior one
  (`DismissOpenModals_`) rather than stacking.
- **Turn gate:** `UIManager::CanAdvanceTurn()` is false when an overlay is open **or** the
  world view reports `BlocksTurnAdvance()` (any open in-view modal). `Engine::ProcessTurn_`
  soft-gates on that query and no-ops when false — it does not throw for an ordinary End Turn
  under a modal. Auto end-turn is queued from `WorldView::Update_` and consumed in
  `UIManager::Update()` (between `ProcessInput` and `Render`), never from the paint path.
- **Council vote Escape:** While `PlanetaryCouncil::GetPending()` is set, Escape does not close
  the vote overlay (Package 5 owns absentee/resolve exits).

### Object Lifetime / Invalidation (code-review 1.8)
Views hold live references/pointers into mutable game state with no generic weak-handle system.
See [`high-level.md`, "Object lifetime and ownership transfer"](high-level.md#object-lifetime-and-ownership-transfer)
for the destroy-vs-transfer protocol these invalidation rules consume. All cases the review
flagged are now closed:

- **`BaseView::HandlePopClick`'s captured `Pop&`** is safe by construction rather than by a
  runtime handle: the only thing that erases a `Pop` (`PopulationManager::RemovePop`, via
  starvation) runs inside `TurnProcessor::Advance`, and `UIManager::CanAdvanceTurn()` refuses
  `Advance` while any overlay (including BaseView) or blocking in-view modal is open. Preserve
  the same "no destructive mutation while a popup holding live references is open" invariant
  for any future mid-turn interactive popup.
- **`WorldView::m_pSelectedUnit`** (a raw `Unit*`) cannot dangle or go foreign: it is cleared on
  `UnitManager::OnUnitDestroyed` (a `Signal<Unit&>` emitted in `DestroyUnit` before the unit is
  erased, mirroring `PopulationManager`'s `OnGrowth`/`OnStarvation` pattern) and on
  `UnitManager::OnUnitReleased` (emitted by `ReleaseUnit` when the unit is about to leave this
  faction via transfer — the address survives, but it is no longer "my" selection). `WorldView`
  connects to every faction's `UnitManager` at construction (all factions exist by then; none
  are added later).
- **`BaseView`/`GrowthDisplay`/`ProductionDisplay`'s `BaseManager&`/`const BaseManager*`**: a
  base can now be destroyed (razed on capture) or change owner (capture / trade / mind-control)
  without the view's knowledge otherwise. `BaseView` connects to
  `BaseManager::OnDestroyed` at construction and pops itself when it fires; the connection is a
  `Signal::ScopedConnection`, which goes inert if the signal dies first, so the view may safely
  outlive the base it was watching. `Render()` also pops
  when `&BaseManager::GetFaction()` no longer matches the owner recorded when the view was
  opened. Ownership transfer is identity-preserving (same `BaseManager` object), so this is a
  polling comparison, not a dangling-pointer check — see `high-level.md`.
