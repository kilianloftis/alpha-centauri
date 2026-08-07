# Input Component Current State

## Current State

- `ac::Input` is a poll-only, non-blocking interface: `PollKey()`, `PollMouse()`,
  `GetLastMousePosition()`. All return `std::optional`; callers drain in a `while` loop.
- There is **one** implementation, `BufferedInput`, in `ac-core`. It reads a
  `PlatformEventQueue` and names no windowing library, so the same class serves both the SFML
  and headless configurations.
- `PlatformEventQueue` is the seam between whatever pumps a window and `Input`. `Engine` owns
  one and passes it to both `CreateGraphics` and `CreateInput`.
- Events carry their modifiers: `PollKey` returns a whole `KeyEvent_t`, not a bare `Key_t`.

## Architecture

- `Graphics::PumpEvents()` (SFML: `SFMLGraphics::PumpInto_`) translates window events and calls
  `PlatformEventQueue::PushKey` / `PushMouse` / `RequestClose`. `Engine::GameLoop_` calls it once
  per frame, before reading input, and separately from `Display()`.
- A window-close request is recorded on the queue; `Engine` takes it and calls
  `UIManager::RequestExit()`. The backend states no close policy.
- `SfmlKeyMapping.cpp` (`USE_SFML` only) holds `KeyFromSfKey`, `MouseButtonFromSfButton` and
  `GetModifierState`. `KeyFromSfKey` returns `nullopt` for an unmapped key, and the pump drops
  the event rather than forwarding an `Unknown` one.

## Dependencies

- `BufferedInput` and `PlatformEventQueue` depend on nothing beyond the standard library and
  live in `ac-core`, so they are unit-testable without a windowing library.
- Only `SfmlKeyMapping.cpp` and `SFMLGraphics.cpp` depend on SFML.

## History

Pending events used to live in file-scope `std::deque`s behind free `PushPendingKeyEvent_t` /
`PopPendingKeyEvent` functions, filled from inside `SFMLGraphics::Display()`. That made the
`Graphics`/`Input` pairing a fiction — `NullGraphics` never pushed, so `NullGraphics` +
`SFMLInput` was a live `Input` that never saw a key — and blocked multi-window use. `NullInput`
was a blocking `std::cin` prompt rather than a null object.
