# UI System Architecture

```mermaid
graph TB
    subgraph "Core Abstractions"
        UIManager[UIManager<br/>(abstract base class)]
        IGameView[IGameView<br/>(interface)]
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
        WorldView[WorldView<br/>implements IGameView]
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
        PopTypeSelectorPopup[PopTypeSelectorPopup<br/>extends UIPopup<br/>lists available pop types]
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
    PopTypeSelectorPopup -.->|extends| UIPopup
    style PopTypeSelectorPopup fill:#bfb,stroke:#333,stroke-width:2px

    subgraph "Unit Designer"
        UnitDesignerView[UnitDesignerView<br/>implements IGameView]
        ComponentSlotDisplay[ComponentSlotDisplay<br/>extends UIElement<br/>shows one component slot]
        ComponentSelectorPopup[ComponentSelectorPopup<br/>extends UIElement<br/>pick component from list]
        DesignStatsDisplay[DesignStatsDisplay<br/>extends UIElement<br/>shows combined stats]
        DesignListPanel[DesignListPanel<br/>extends UIElement<br/>shows saved designs]
        UnitStatusPanel[UnitStatusPanel<br/>extends UIElement<br/>shows active/in-prod counts]
        UnitDesignerState[UnitDesignerState_t<br/>draft component selection]
    end
    UnitDesignerView -->|owns| ComponentSlotDisplay
    UnitDesignerView -->|owns| DesignStatsDisplay
    UnitDesignerView -->|owns| DesignListPanel
    UnitDesignerView -->|owns| UnitStatusPanel
    UnitDesignerView -->|spawns on click| ComponentSelectorPopup
    UnitDesignerView -->|holds| UnitDesignerState
    DesignStatsDisplay -->|reads| UnitDesignerState
    ComponentSlotDisplay -->|reads via lambda| UnitDesignerState
    UnitDesignerView -->|saves to| Military
    DesignListPanel -->|reads| Military

    style UnitDesignerView fill:#bfb,stroke:#333,stroke-width:2px
    style ComponentSlotDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style ComponentSelectorPopup fill:#bfb,stroke:#333,stroke-width:2px
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

### Render Order
Views are rendered bottom-to-top through the stack. Each view renders its own `UIElement`s in its `Render()` method.

### WorldDisplay Viewport
- **Purpose**: Controls which portion of the world map is visible on screen.
- **State**: `m_tileSize` (pixel size per tile), `m_cameraX`/`m_cameraY` (top-left tile coordinates of the viewport).
- **Rendering**: `Render(x, y, w, h)` computes how many tiles fit in the given pixel area from `m_tileSize`, then renders only the tile range `[cameraX, cameraX + tilesWide) x [cameraY, cameraY + tilesHigh)`. Layers are drawn in this order: tiles, bases, then units.
- **Configurability**: `SetTileSize()` and `SetCameraOffset()` are the sole control points. Tile size is intentionally configurable to support zoom and per-platform tuning.
- **Mouse hit-testing**: `WorldView::HandleMouse` reads the viewport state from `WorldDisplay` and translates screen-relative tile indices back to world tile coordinates by adding the camera offset.
- **Unit Layer**: Unit markers are rendered on top of bases by querying `WorldMap::GetUnitsOnTile()` for each visible tile. Multiple units on the same tile are drawn side-by-side; faction coloring is a future TODO.
- **Unit Selection**: Left-clicking a tile with units selects the first unit on that tile (`WorldView::SelectUnitAtTile_`). The selected unit is highlighted with a yellow border and is passed to `WorldDisplay` via `SetSelectedUnit()`. If the tile has no units, the click falls back to opening a base.
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
- **Methods**:
  - `CreateWorldView(...)`: Builds the world view
  - `CreateBaseView(...)`: Builds a base view for the selected base
  - `CreateResearchView(...)`: Builds the research view for the player faction

### Factory: CreateUIManager()
Returns `UIManagerImpl`, a platform-agnostic implementation (no compile-time flag needed).

### Object Lifetime / Invalidation (code-review 1.8)
Views hold live references/pointers into mutable game state with no generic weak-handle system.
Two of the three cases the review flagged are closed; the third is deliberately deferred:

- **`BaseView::HandlePopClick`'s captured `Pop&`** is safe by construction rather than by a
  runtime handle: the only thing that erases a `Pop` (`PopulationManager::RemovePop`, via
  starvation) runs inside `TurnProcessor::ProcessTurn`, reachable only through
  `WorldView::HandleKey`'s `Enter` case — and `UIManager::GetActiveView_()` routes all input to
  the top of the overlay stack, so `WorldView::HandleKey` cannot fire while `BaseView` (or a
  popup it owns) is pushed. `Engine::ProcessTurn_` asserts this invariant explicitly via
  `UIManager::HasOverlayView()` (throws `std::logic_error` if an overlay is active) so a future
  caller that bypasses view-stack routing fails loudly instead of corrupting a popup's captured
  reference. A future mid-turn-stage popup (prompting for input before a stage continues, not
  implemented yet) will need to preserve this same "no destructive mutation while a popup holding
  live references is open" invariant.
- **`WorldView::m_pSelectedUnit`** (a raw `Unit*`) is kept valid via `UnitManager::OnUnitDestroyed`,
  a `Signal<Unit&>` emitted in `DestroyUnit` before the unit is erased (mirroring
  `PopulationManager`'s `OnGrowth`/`OnStarvation` pattern). `WorldView` connects to every
  faction's `UnitManager` at construction (all factions exist by then; none are added later) and
  clears `m_pSelectedUnit` when it matches the destroyed unit.
- **Deferred**: `BaseView`/`GrowthDisplay`/`ProductionDisplay` hold `BaseManager&`/`const
  BaseManager*` for their whole life with no invalidation path. No code destroys a `BaseManager`
  today — base capture/destruction doesn't exist as a feature — so there is nothing to guard
  against yet; revisit once that feature lands.
