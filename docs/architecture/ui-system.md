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
