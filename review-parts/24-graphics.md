## Graphics backend

**Files:** `src/graphics/NullGraphics.cpp`, `src/graphics/SFMLGraphics.cpp`,
`include/graphics/Graphics.h`

**Assessment:** The `Graphics` interface is small and usable — UI code draws through
references to an abstract surface, and `SFMLGraphics` correctly owns its
`sf::RenderWindow` / font / texture map and throws if the window fails to open (prior
4.2 residue here is closed). The dominant weakness is that the SFML backend is not a
renderer: `Display()` is also the input pump, close-policy owner, and maximize hack, so
the declared Graphics/Input split is fiction. Secondary issues are silent degradation
(font load) and presentation constants that belong in config.

### [H] `Display()` secretly owns the input pipeline and window-close policy
`src/graphics/SFMLGraphics.cpp:131-135`, `223-268` — every frame, `Display()` calls
`ProcessEvents_()`, which drains the SFML event queue into file-scope
`PushPendingKeyEvent_t` / `PushPendingMouseEvent_t` globals and deliberately swallows
`sf::Event::Closed` ("only Enter should close"). Consequences: (1) `Graphics` and
`Input` are not interchangeable backends — `NullGraphics` never pushes, so any pairing
with `SFMLInput` yields dead input; (2) a render call has hidden I/O side effects that
preclude multiple windows and clean unit tests of drawing; (3) player-facing close
policy lives in the graphics TU instead of the engine/UI layer. Prior finding 4.6
recorded this and is still open; the graphics half of the fix is to stop pumping input
from `Display()` (hand the native window / poll loop to `Input`, or to a shared
window owner both backends receive).

### [M] Font load failure silently disables all text drawing
`src/graphics/SFMLGraphics.cpp:117-123`, `164-168` — if both hard-coded system font
paths fail, the constructor only logs to `stderr` and continues; `DrawText` then
returns early when `m_font.getInfo().family` is empty. The entire current UI is
text/rect based, so a missing distro font produces a black window with no labels and
no throw — opposite of the project rule to prefer exceptions over silent defaults.
Prior §9 noted this; still unfixed. Direction: throw from the constructor (or take a
configured font path and fail loud), and stop treating "no font" as a valid ready state.

### [M] Presentation settings are hard-coded in the SFML TU
`src/graphics/SFMLGraphics.cpp:36-39`, `83-84`, `89-92`, `113` — window size
(1280×900), title, 60 FPS cap, maximize wait policy, and two Debian-centric font paths
are compile-time literals with no config or constructor parameters. `NullGraphics`
duplicates the size constants (`src/graphics/NullGraphics.cpp:56-63`) so headless
layout math stays aligned only by copy. Prior §9; still open. Direction: pass a
`GraphicsConfig_t` (or equivalent) into `CreateGraphics` / the constructors so size,
title, FPS, and font path are data.

### [M] `NullGraphics` fails texture ops that a null object should no-op successfully
`src/graphics/NullGraphics.cpp:27-36` — `LoadTexture` / `DrawSprite` log and return
`false`. A substitutable null backend should accept loads and draws as successful
no-ops; returning failure forces every future caller to special-case headless mode and
breaks LSP relative to a backend that can satisfy the same calls. (No production caller
exists yet — both APIs are unused outside these files — but the contract is already
wrong.) Direction: return `true` from the null implementations (and consider throwing
from SFML on load failure per guidelines, instead of `bool`).

### [M] `LoadTexture` does not replace an existing id
`src/graphics/SFMLGraphics.cpp:145` — `m_textures.emplace(id, …)` leaves the previous
texture in place when `id` is already present, while still returning `true` after a
successful file load. Callers that reload/replace an asset will observe the old GPU
data with no error. Direction: `insert_or_assign` (or erase-then-emplace) after a
successful load.

### [L] Convention and hygiene items
- `src/graphics/NullGraphics.cpp:14-64` — method definitions are indented at column 0
  inside the class (brace/indent drift vs `SFMLGraphics`).
- `src/graphics/NullGraphics.cpp:39`, `src/graphics/SFMLGraphics.cpp:164` — default
  arguments repeated on overrides; only the base defaults in
  `include/graphics/Graphics.h:34-37` apply through a `Graphics*`.
- `src/graphics/SFMLGraphics.cpp:238` — local `auto KeyEvent_t` uses a type name as a
  variable (shadows the type; should be `pKeyEvent` / `keyPressed`).
- `src/graphics/SFMLGraphics.cpp:83-84` — `k_FontPath1` / `k_FontPath2` sit outside the
  anonymous namespace with external linkage in this TU; `SFMLGraphics` itself is also
  not hidden in an anonymous namespace (unlike `NullGraphics`).
- `src/graphics/NullGraphics.cpp:16` — logs "Null graphics backend" while prior review
  correctly noted the name oversells headless capability (console chatter, fixed size).
- `include/graphics/Graphics.h:17-22` — `Color_t` factories use same-line braces,
  contrary to the project brace rule.

**Observed outside slice:**
- `docs/architecture/graphics-system.md:7,63,73` — still documents `Initialize()`, an
  800×600 window, and `SFMLKeyEventQueue`; the code has no `Initialize`, defaults to
  1280×900, and pushes into `KeyEventQueue`/`MouseEventQueue`.
- `src/ui/ViewFactory.cpp:178-186` — fullscreen layout is snapshotted from
  `GetWindowWidth`/`Height` once; `SFMLGraphics` resize updates the SFML view
  (`SFMLGraphics.cpp:233-236`) but UI pixel layouts do not invalidate (prior 4.5).
