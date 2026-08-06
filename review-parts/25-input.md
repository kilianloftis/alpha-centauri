## Input backend

**Files:** `src/input/KeyEventQueue.cpp`, `include/input/KeyEventQueue.h`,
`src/input/KeyMapping.cpp`, `include/input/KeyMapping.h`,
`src/input/MouseEventQueue.cpp`, `include/input/MouseEventQueue.h`,
`src/input/NullInput.cpp`, `src/input/SFMLInput.cpp`, `include/input/Input.h`

**Assessment:** The `Input` interface and the SFML/console split are small and readable, and
`KeyEvent_t` correctly carries modifiers with the keystroke. The dominant weakness is that
"input" is not a self-contained backend: pending events live in file-scope globals filled by
the graphics layer, so the abstract `Input`/`Graphics` pairing is a fiction. Secondary
issues are an `optional`-returning mapper that never returns empty, and an Async API that is
neither asynchronous nor substitutable across backends. Prior finding 4.6 is still open.

### [H] Own pending key/mouse events inside the input backend
`src/input/KeyEventQueue.cpp:7-11`, `src/input/MouseEventQueue.cpp:7-15`,
`src/input/SFMLInput.cpp:26-43` — events sit in process-global `static std::deque`s with free
`Push`/`Pop` functions; `SFMLInput` only drains them. Push sites are in
`SFMLGraphics::ProcessEvents_`, so pairing `NullGraphics` with `SFMLInput` yields a live
`Input` object that never sees a key or click. Globals also block multi-window use and force
tests to share one mutable queue. Same structural defect as prior finding 4.6 (still
unresolved). Direction: make the queues members of `SFMLInput` (or inject an event source
the graphics backend writes into), and stop exposing free push/pop as the integration seam.

### [H] `KeyFromSfKey` never returns `nullopt` — unmapped keys become `Unknown` events
`src/input/KeyMapping.cpp:209-210` — the default branch is `return Key_t::Unknown;`, so the
`std::optional<Key_t>` is always engaged. Callers that correctly write
`if (auto mapped = KeyFromSfKey(...))` (e.g. `SFMLGraphics.cpp:240-242`) therefore push a
`KeyEvent_t{Unknown, …}` for every unmapped key (Tab, Backspace, punctuation, etc.).
`KeyFromAscii` and `MouseButtonFromSfButton` return `nullopt` for unknowns; this mapper
lies about the same contract. Direction: `return std::nullopt` in the default branch (and
never push `Unknown` unless a real unknown key event is intentional).

### [M] `CaptureKey` discards modifiers that `CaptureKeyAsync` preserves
`include/input/Input.h:68-69`, `src/input/SFMLInput.cpp:24-38` — `CaptureKey` returns
`optional<Key_t>` after popping a full `KeyEvent_t` and throwing away `modifier`, while
`CaptureKeyAsync` delivers the whole event. `KeyEvent_t`'s own comment
(`include/input/Input.h:41-43`) says consumers must read modifiers from the event rather
than polling. Any caller that needs chords is forced onto the misnamed Async API; any caller
that uses `CaptureKey` silently loses Ctrl/Alt/Shift. Direction: make `CaptureKey` return
`optional<KeyEvent_t>` (mirror `CaptureMouse`) and drop the key-only overload.

### [M] `*Async` methods are synchronous and not substitutable across backends
`src/input/SFMLInput.cpp:33-38`, `src/input/NullInput.cpp:30-39` — neither implementation
schedules work; both run inline. Worse, behavior diverges under LSP: SFML invokes the
callback only when a pending event exists; Null always blocks on `stdin` and still invokes
the callback with `Key_t::Unknown` / `MouseButton_t::None` on failure
(`NullInput.cpp:36-38`, `76-78`). A loop written against SFML's "no callback means empty"
semantics misbehaves under Null, and vice versa. Direction: rename to try/poll semantics
(or drain-all with a real queue on the object), and make both backends agree on
callback-vs-empty rules — Null should not synthesize fake events.

### [M] `NullInput` is a blocking console backend, not a null/headless object
`src/input/NullInput.cpp:18-27`, `42-49` — `CaptureKey`/`CaptureMouse` print prompts and
block on `std::cin`. Named and factory-selected as the non-SFML `Input`, this stalls any
frame loop that polls input every tick; it is unsuitable for automated/headless runs.
Direction: rename to `ConsoleInput` (matching its own log line) and provide a true no-op
`Input` that returns empty optionals without blocking, if headless is a real requirement.

### [M] Hand-rolled key maps drift; `Key_tToString` omits F1–F12
`src/input/KeyMapping.cpp:98-146` vs `150-210` — three parallel switches
(`KeyFromAscii`/`KeyToAscii`/`Key_tToString`/`KeyFromSfKey`) must be edited together when
`Key_t` grows. `Key_tToString` has no cases for `F1`–`F12` (defined at
`include/input/Input.h:19`) and falls through to `"Unknown"`, while `KeyFromSfKey` maps
those keys. Guidelines prefer `magic_enum` when the string form matches the enumerator;
`Key_tToString` is exactly that case. Direction: implement `Key_tToString` via
`magic_enum::enum_name` (keep explicit maps only for ASCII/SFML wire forms).

### [L] Convention and hygiene items
- `include/input/KeyEventQueue.h:9`, `include/input/MouseEventQueue.h:15` —
  `PushPendingKeyEvent_t` / `PushPendingMouseEvent_t` put the `_t` type suffix on functions
  (called out in prior 4.6; still present).
- `include/input/KeyMapping.h:17` — `Key_tToString` mangles a type suffix into a function
  name; prefer `KeyToString`.
- `include/input/Input.h:23-29` — `Modifier_t` is unused; only `ModifierState_t` is live.
- `include/input/Input.h:69`, `include/input/KeyMapping.h:15,19`,
  `src/input/SFMLInput.cpp:24` — missing space before `CaptureKey` / `KeyFromAscii` /
  `KeyFromSfKey` declarators (`std::optional<Key_t>CaptureKey`).
- `src/input/KeyMapping.cpp:7-49` — switch bodies are column-0 indented; inconsistent with
  project brace/indent style.
- `src/input/SFMLInput.cpp:7-12` — unused includes (`chrono`, `thread`, `vector`, SFML
  Keyboard/Mouse) left after the old polling `CaptureMouse` was removed.
- `include/input/MouseEventQueue.h:18`, `src/input/MouseEventQueue.cpp:23-25` —
  `GetLastMousePosition` returns `{0,0}` when never set instead of throwing; callers must
  remember `HasLastMousePosition` first.
- No tests under `tests/` exercise queues, mapping, or either backend for implemented
  poll/drain behavior.

**Observed outside slice:**
- `src/ui/UIManager.cpp:42-52` vs `74-83` — `ProcessKeys_` invokes `CaptureKeyAsync` once
  per frame (at most one key) while `ProcessMouse_` drains the full mouse queue; fast typing
  can backlog in the global key deque indefinitely.
- `src/graphics/SFMLGraphics.cpp:228-231` — window close is swallowed inside the graphics
  backend ("only Enter should close"); interaction policy buried outside Input.
- `docs/architecture/input-system.md` — still documents `Initialize()`, a blocking
  SFML `CaptureMouse` poll loop, and `SFMLKeyEventQueue` naming that no longer match the
  code.
