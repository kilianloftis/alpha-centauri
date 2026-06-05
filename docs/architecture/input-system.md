# Input System Architecture

```mermaid
graph TB
    subgraph "Input Interface"
        Input[Input<br/>(abstract base class)]
        Methods[Virtual Methods:<br/>Initialize()<br/>CaptureKey()<br/>CaptureKeyAsync()<br/>CaptureMouse()<br/>CaptureMouseAsync()]
    end

    subgraph "SFML Implementation"
        SFMLInput[SFMLInput]
        SFMLKeyboard[SFML Keyboard polling]
        SFMLMouse[SFML Mouse polling]
        AsyncCallbacks[Async callback support]
    end

    subgraph "Null Implementation"
        NullInput[NullInput]
        ConsoleInput[Console stdin input]
    end

    subgraph "Key Mapping"
        KeyMapping[KeyMapping]
        KeyFromAscii[KeyFromAscii()]
        KeyToAscii[KeyToAscii()]
        KeyToString[KeyToString()]
        KeyFromSfKey[KeyFromSfKey()]
    end

    subgraph "Event Queue"
        SFMLKeyEventQueue[SFMLKeyEventQueue]
        PushPending[PushPendingKeyEvent()]
        PopPending[PopPendingKeyEvent()]
        Queue[Internal event queue]
    end

    subgraph "Factory"
        Factory[CreateInput()]
        CompileFlag[USE_SFML<br/>compile-time flag]
    end

    subgraph "Data Types"
        KeyEnum[Key enum<br/>A-Z, 0-9, Space, Escape, Enter]
        KeyEvent[KeyEvent struct<br/>Key key]
        MouseButtonEnum[MouseButton enum<br/>Left, Right, Middle]
        MouseEvent[MouseEvent struct<br/>MouseButton button<br/>int x, int y]
    end

    Input --> Methods
    SFMLInput -.->|implements| Input
    NullInput -.->|implements| Input

    SFMLInput --> SFMLKeyboard
    SFMLInput --> SFMLMouse
    SFMLInput --> AsyncCallbacks

    SFMLInput --> KeyMapping
    SFMLInput --> SFMLKeyEventQueue

    KeyMapping --> KeyFromAscii
    KeyMapping --> KeyToAscii
    KeyMapping --> KeyToString
    KeyMapping --> KeyFromSfKey

    SFMLKeyEventQueue --> PushPending
    SFMLKeyEventQueue --> PopPending
    SFMLKeyEventQueue --> Queue

    Factory -->|if USE_SFML defined| SFMLInput
    Factory -->|if USE_SFML not defined| NullInput
    Factory --> CompileFlag

    NullInput --> ConsoleInput
    NullInput --> KeyMapping

    Input --> KeyEnum
    Input --> KeyEvent
    Input --> MouseButtonEnum
    Input --> MouseEvent

    style Input fill:#bbf,stroke:#333,stroke-width:4px
    style SFMLInput fill:#bfb,stroke:#333,stroke-width:2px
    style NullInput fill:#fbb,stroke:#333,stroke-width:2px
    style KeyMapping fill:#ff9,stroke:#333,stroke-width:2px
    style SFMLKeyEventQueue fill:#ff9,stroke:#333,stroke-width:2px
    style Factory fill:#ff9,stroke:#333,stroke-width:2px
```

## Component Overview

### Input (Abstract Base Class)
- **Purpose**: Defines the interface for input handling operations
- **Virtual Methods**:
  - `Initialize()`: Initialize the input backend
  - `CaptureKey()`: Synchronously capture a key press
  - `CaptureKeyAsync(callback)`: Asynchronously capture a key press with callback
  - `CaptureMouse()`: Synchronously capture a mouse click
  - `CaptureMouseAsync(callback)`: Asynchronously capture a mouse click with callback

### SFMLInput
- **Purpose**: SFML-based input implementation
- **Components**:
  - `SFML Keyboard polling`: Polls SFML keyboard state
  - `SFML Mouse polling`: Polls SFML mouse state with position tracking
  - `Async callback support`: Wraps synchronous calls with callbacks
- **Behavior**:
  - `CaptureKey()`: Pops pending key event from SFMLKeyEventQueue
  - `CaptureMouse()`: Blocking loop polling mouse buttons until click detected
  - Returns mouse position with button type
  - Supports Left, Right, and Middle mouse buttons

### NullInput
- **Purpose**: Console-based input implementation for testing/headless mode
- **Behavior**:
  - `CaptureKey()`: Reads character from stdin, converts to Key via KeyFromAscii
  - `CaptureMouse()`: Reads "x y" or "n" from stdin for mouse coordinates
  - Used when `USE_SFML` is not defined
  - Allows game logic testing without graphics window

### KeyMapping
- **Purpose**: Converts between different key representations
- **Functions**:
  - `KeyFromAscii(char)`: Convert ASCII character to Key enum
  - `KeyToAscii(Key)`: Convert Key enum to ASCII character
  - `KeyToString(Key)`: Convert Key enum to string representation
  - `KeyFromSfKey(sf::Keyboard::Key)`: Convert SFML key to Key enum (SFML only)
- **Usage**: Used by both SFMLInput and NullInput for key conversion

### SFMLKeyEventQueue
- **Purpose**: Queues key events from SFML graphics system
- **Functions**:
  - `PushPendingKeyEvent(Key)`: Add key event to queue
  - `PopPendingKeyEvent()`: Remove and return key event from queue
- **Integration**: Called by SFMLGraphics during event processing
- **Flow**: SFML window events → SFMLGraphics → SFMLKeyEventQueue → SFMLInput

### CreateInput() Factory
- **Purpose**: Factory function to create appropriate input implementation
- **Selection**: Based on `USE_SFML` compile-time flag
  - If defined: Returns `SFMLInput`
  - If not defined: Returns `NullInput`

### Data Types
- **Key enum**: Represents keyboard keys (A-Z, 0-9, Space, Escape, Enter, Unknown)
- **KeyEvent struct**: Contains a Key field
- **MouseButton enum**: Represents mouse buttons (Left, Right, Middle, None)
- **MouseEvent struct**: Contains MouseButton and x, y coordinates
