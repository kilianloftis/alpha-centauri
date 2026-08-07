# Package 13 — Platform layer: event bus, graphics, input backends

Findings re-verified against the tree at `f2265de`.

## Verified diagnoses

### [H] `EventBus::Publish` iterates the live handler list

`src/lib/EventBus.cpp:23-26` — `for (const auto& [_, h] : m_handlers) h(event);`. Verified: a
handler that `Subscribe`s reallocates the vector mid-iteration, and one that `Unsubscribe`s
erases from it. `Signal::Emit` (`include/lib/Signal.h:124-134`) already snapshots and re-checks
each id before invoking; the bus, which the architecture docs call the mod-facing ABI, does not.

**Chosen:** the same treatment as `Signal` — snapshot, then skip ids unsubscribed since the
snapshot was taken. Handlers added during dispatch are not called for the in-flight event.

### [H] `Display()` owns the input pipeline and the window-close policy

`src/graphics/SFMLGraphics.cpp:129-133` (`Display` → `ProcessEvents_`) and `:223-268`. Verified
all three consequences:

- `ProcessEvents_` writes to the file-scope queues behind `PushPendingKeyEvent_t` /
  `PushPendingMouseEvent_t`, and `SFMLInput` only drains them. `NullGraphics` never pushes, so
  `NullGraphics` + `SFMLInput` is a live `Input` that never sees a key. The two backends are not
  independently substitutable.
- Rendering has hidden I/O side effects.
- `sf::Event::Closed` is swallowed in the graphics TU with the comment "only Enter should close" —
  a player-facing policy decided three layers below the UI.

**Chosen:** an explicit `PlatformEventQueue` owned by the composition root, written by whichever
backend pumps a window and read by whichever backend implements `Input`. `Graphics` grows
`PumpEvents()`, which `Engine` calls once per frame before `Clear()`; `Display()` only presents.
A close request becomes an entry on the queue (`TakeCloseRequest()`), so the engine decides what
to do about it and the graphics backend states no policy.

**Rejected:** keeping the free push/pop functions and merely moving the storage into `SFMLInput`.
It fixes the globals but not the coupling — graphics would still have to reach into a specific
`Input` implementation, which is what makes the pairing rigid.

### [H] `KeyFromSfKey` never returns `nullopt`

`src/input/KeyMapping.cpp:209` — `default: return Key_t::Unknown;`. Verified: every caller that
correctly writes `if (auto mapped = ...)` pushes a `KeyEvent_t{Unknown}` for Tab, Backspace,
punctuation and every other unmapped key. `KeyFromAscii` and `MouseButtonFromSfButton` return
`nullopt` for unknowns, so this mapper alone breaks the shared contract.

### [M] Font load failure silently disables all text

`SFMLGraphics.cpp:115-122` logs to `stderr` and continues; `DrawText:161-165` then returns early
forever. Verified — the whole UI is text and rectangles, so a missing distro font is a black
window with no diagnostic.

### [M] Presentation settings are compile-time literals

`SFMLGraphics.cpp:36-39, 82-83, 89-92, 112` and the duplicated 1280×900 in
`NullGraphics.cpp:56-63`. Verified: window size, title, FPS cap, maximize wait, and two
Debian-specific font paths.

**Chosen:** a `GraphicsConfig_t` passed to `CreateGraphics`, with the font as a *list* of
candidate paths so a mod or a distro can add its own without losing the fallbacks. Both backends
read the same size, so headless layout math stops being aligned by copy.

### [M] `NullGraphics` fails texture ops; `LoadTexture` does not replace

`NullGraphics.cpp:27-36` returns `false` from `LoadTexture`/`DrawSprite` — a null object that
reports failure forces every caller to special-case headless. `SFMLGraphics.cpp:143` uses
`emplace`, so reloading an id keeps the old texture and still returns `true`. Both verified.

### [M] `CaptureKey` discards modifiers; `*Async` is neither async nor substitutable

`include/input/Input.h:66-69`, `SFMLInput.cpp:22-38`, `NullInput.cpp:30-39`. Verified:
`CaptureKey` returns `optional<Key_t>` after popping a full `KeyEvent_t`, contradicting
`KeyEvent_t`'s own comment that consumers must read modifiers from the event. Neither `*Async`
schedules anything, and they disagree under LSP — SFML invokes the callback only when an event
exists, Null always blocks on `stdin` and then invokes it with a synthesized
`Key_t::Unknown` on failure.

**Chosen:** `PollKey()` / `PollMouse()` returning `std::optional<KeyEvent_t>` /
`std::optional<MouseEvent_t>`, and nothing else. The callback forms go away: the one consumer
(`UIManager::ProcessKeys_`) is a `while` loop over a queue, which reads better as a loop.

### [M] `NullInput` is a blocking console backend

`NullInput.cpp:18-27, 42-49` prompts and blocks on `std::cin`. Verified. It is selected as *the*
non-SFML `Input`, so a headless run stalls the frame loop on the first tick.

**Chosen:** make it a genuine null — return `nullopt`, never block, never synthesize. **Rejected:**
renaming it to `ConsoleInput` and adding a separate null. Nothing in the tree drives the game from
a console prompt, and the guidelines forbid keeping code with no current requirement; if
interactive console input is wanted later it can be written then.

### [M] Hand-rolled key maps drift; `Key_tToString` omits F1–F12

`KeyMapping.cpp:98-146` vs `:150-210`. Verified: `Key_tToString` has no `F1`–`F12` cases and
falls through to `"Unknown"` while `KeyFromSfKey` maps them. The enumerator names already match
the intended strings, so `magic_enum` removes the second source of truth entirely.

## Review follow-ups

The review verified the mechanical risk I was most worried about — `SfmlKeyMapping.cpp` was
extracted by a shell pipeline, and it confirmed all three functions present once, all 52 case
labels in order, braces balanced, nothing lost at the split. It also confirmed the font throw is
caught by `main`'s try/catch. What it found:

1. **[Regression] The headless build became a 100%-CPU spin loop.** `NullInput` used to block on
   `std::cin`, which idled the frame loop at 0%. With a genuine null input and a no-op
   `Display()`, nothing paced the loop and nothing could ever set the exit flag — a CI run would
   pin a core until timeout, which is *worse* for the package's own stated goal of making
   headless usable. `NullGraphics::Display()` now paces to `framerateLimit`, since SFML's
   `setFramerateLimit` is the only pacing in the windowed build.

2. **The two `Input` backends had become the same class.** Once `SFMLInput` stopped touching
   SFML, it and `NullInput` differed only by a log line — two files kept in sync by hand for no
   behavioural difference, both outside the test target. Collapsed into one `BufferedInput` in
   `ac-core`, which also gives `Input` its first test coverage.

3. **The whole portable half of `KeyMapping` was dead.** `KeyFromAscii`'s only caller was
   `NullInput`'s console reader, which this package deleted; `KeyToAscii` and `Key_tToString`
   already had none. I had *promoted* that TU into `ac-core`, added a `magic_enum` dependency to
   it, and written tests for it. Deleted instead, per the guideline against code with no current
   requirement.

   This means the `[M]` "`Key_tToString` omits F1–F12" finding is resolved **by deletion**, not by
   fixing the function. Nothing in the game renders a key name, so the drift it described was
   unobservable. A future keybinding display will need to write this, and should use `magic_enum`
   rather than a fourth hand-written switch.

4. **`GraphicsConfig_t` claimed configurability it did not have.** The literals moved from a
   `.cpp` to a `.h` and every call site used the default, so the finding was structurally rather
   than actually fixed. Now loaded from a `graphics` block in `user_settings.json`, which meant
   moving `GameSettings::Load` ahead of backend construction in `Engine` — the window is opened
   from those values.

5. **`Render()` still pulled from `Input` and drove the camera.** I lifted `PumpEvents` out of
   `Display()` but left the equivalent coupling one layer up; edge scrolling moved to `Update()`.

6. Smaller: the close path now goes through `UIManager::RequestExit()` rather than `break`ing
   past the exit flag; the constructor's maximize wait calls a non-virtual `PumpInto_`;
   `EventBus::Publish` snapshots ids rather than deep-copying every `std::function` per event;
   the unused `Modifier_t` enum, a dead `#ifdef` guard, and a vacuous `KeyFromAscii` test are
   gone.

## Test coverage note

`KeyFromSfKey`, `MouseButtonFromSfButton` and `GetModifierState` are **not** covered. They live
behind `USE_SFML`, which is defined only on the executable target; the test target links
`ac-core`, which has no SFML dependency. Adding SFML to the test link to reach three mapping
functions is a worse trade than leaving them uncovered.

Nor is anything that *drives* them: no `Graphics` implementation is in the test target, so
nothing would catch `SFMLGraphics::PumpEvents` forgetting to call `PushKey`, or
`Engine::GameLoop_` losing its `PumpEvents()` call. The queue and `Input` are tested; the wiring
between them and a real window is not.

The `EventBus` subscribe-during-dispatch test pins the resulting contract but cannot prove the
undefined behaviour is gone — reading a reallocated vector may happen to work, and it did pass
against the unfixed code. The unsubscribe-during-dispatch test is the one that fails
deterministically without the fix.

## Scope note

**Superseded by follow-up 3 — `KeyFromAscii`, `KeyToAscii` and `Key_tToString` are deleted, not
kept.** The original reasoning was:

`KeyFromAscii` / `KeyToAscii` stay hand-written: an ASCII code is not the enumerator name, so
`magic_enum` does not apply. They are one map each and are not the pair that drifted.
