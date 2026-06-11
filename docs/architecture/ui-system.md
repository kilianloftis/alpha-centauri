# UI System Architecture

```mermaid
graph TB
    subgraph "UI Abstractions"
        UIManager[UIManager<br/>(abstract base class)]
        UIElement[UIElement<br/>(abstract base class)]
        UIWorldMap[UIWorldMap<br/>extends UIElement]
        UIPanel[UIPanel<br/>extends UIElement]
        UIPopup[UIPopup<br/>extends UIElement]
    end

    subgraph "SFML Implementation"
        SFMLUIManager[SFMLUIManager]
        SFMLWorldMap[SFMLWorldMap]
        SFMLInfoPanel[SFMLInfoPanel]
        SFMLPopup[SFMLPopup]
    end

    subgraph "Null Implementation"
        NullUIManager[NullUIManager]
        NullWorldMap[NullWorldMap]
        NullInfoPanel[NullInfoPanel]
        NullPopup[NullPopup]
    end

    subgraph "Factory"
        Factory[CreateUIManager()]
        CompileFlag[USE_SFML<br/>compile-time flag]
    end

    subgraph "Dependencies"
        Graphics[Graphics]
        Input[Input]
    end

    UIManager --> UIWorldMap
    UIManager --> UIPanel
    UIManager --> UIPopup

    UIWorldMap --> UIElement
    UIPanel --> UIElement
    UIPopup --> UIElement

    SFMLUIManager -.->|implements| UIManager
    SFMLWorldMap -.->|implements| UIWorldMap
    SFMLInfoPanel -.->|implements| UIPanel
    SFMLPopup -.->|implements| UIPopup

    NullUIManager -.->|implements| UIManager
    NullWorldMap -.->|implements| UIWorldMap
    NullInfoPanel -.->|implements| UIPanel
    NullPopup -.->|implements| UIPopup

    SFMLUIManager --> Graphics
    SFMLUIManager --> Input

    Factory -->|if USE_SFML defined| SFMLUIManager
    Factory -->|if USE_SFML not defined| NullUIManager
    Factory --> CompileFlag

    style UIManager fill:#bbf,stroke:#333,stroke-width:4px
    style UIElement fill:#bbf,stroke:#333,stroke-width:2px
    style SFMLUIManager fill:#bfb,stroke:#333,stroke-width:2px
    style NullUIManager fill:#fbb,stroke:#333,stroke-width:2px
    style Factory fill:#ff9,stroke:#333,stroke-width:2px
```

## Component Overview

### UIElement (Abstract Base Class)
- **Purpose**: Base class for all UI elements
- **Properties**: position (x, y), size (width, height), visibility
- **Virtual Methods**:
  - `Draw(Graphics&)`: Render the element
  - `Update(float deltaTime)`: Update element state

### UIWorldMap
- **Purpose**: Renders the world map as the bottom-most UI layer
- **Extends**: UIElement
- **Current state**: Placeholder dark green rectangle; will be replaced with terrain tile sprites

### UIPanel
- **Purpose**: Information panel at the bottom of the screen
- **Extends**: UIElement with a title property
- **Current state**: Basic skeleton bar; content to be added later

### UIPopup
- **Purpose**: Modal popup that displays text and a dismiss button
- **Extends**: UIElement with text, onDismiss callback
- **Virtual Methods**:
  - `Dismiss()`: Hide the popup and invoke the callback

### UIManager (Abstract Base Class)
- **Purpose**: Owns and layers all UI elements, provides the public API
- **Virtual Methods**:
  - `Initialize(Graphics&)`: Set up layout
  - `Draw(Graphics&)`: Render all layers in order (world map, info panel, popup)
  - `Update(float)`: Tick all elements
  - `HandleInput(Input&)`: Process input for active popup
  - `ShowPopup(text, onDismiss)`: Display a popup
  - `DismissPopup()`: Hide the active popup
  - `HasActivePopup()`: Query popup state

### Draw Layer Order
1. **UIWorldMap** (bottom)
2. **UIPanel** (info bar)
3. **UIPopup** (top, when visible)

### Factory: CreateUIManager()
- **Selection**: Based on `USE_SFML` compile-time flag
  - If defined: Returns `SFMLUIManager`
  - If not defined: Returns `NullUIManager`
