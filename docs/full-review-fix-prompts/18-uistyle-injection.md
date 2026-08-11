# Package 18 — UiStyle: from process global to injected per-feature style

The last unaddressed `[H]` in the code review. Measured against the tree at `7053eb4`.

## The finding

> **[H] Stop growing a process-global god-object style registry.** `UiStyle` is both a ~36-member
> typed bag and a file-scope singleton accessed via `Style()`. Every new panel requires a new
> nested struct, a `UiStyle` member, a `Parse*Style_` clone, and another `root.at(...)` line in
> `Load`. That violates open/closed growth and bypasses the project's owned definition-data
> pattern (`GameDataContext`): UI code cannot take a `const UiStyle&` at construction, and tests
> cannot inject an alternate theme without mutating process state.

## Why it was deferred, and why that no longer applies

Deferred three times, each with the same reason: *the UI had no automated coverage, so a sweep of
this size could only be eyeballed.* That reason is gone. Package 15 built `ac-ui` (the UI is a
backend-free library the test target links), `ViewFixture` (a live session and a real
`ViewFactory`), and `RecordingGraphics` (records draw positions **and** colours). Views, panels
and popups are now constructed and driven in tests, so a conversion can be verified.

What remains is size — and the measurements below say it is a smaller job than the finding
implies.

## Measured, not assumed

| | |
|---|---|
| `Style()` call sites | **268** |
| Files containing one | **53** |
| Style structs in `UiStyle` | **36** |
| `include/ui/style/UiStyle.h` | 592 lines |
| `src/ui/style/UiStyle.cpp` | 710 lines |
| `Style()` used outside `src/ui/` | **none** |
| `UiStyle::Load` callers | 2 (`Engine`, `ViewFixture`) + 2 config tests |

**The number that reframes the package:** almost every file reads exactly **one** section.

| Distinct sections read | Files |
|---|---|
| 4 | 1 (`WorldView`) |
| 3 | 4 (`UnitDesignerView`, `CouncilVoteView`, `CommlinksView`, `BaseView`) |
| 2 | ~7 |
| 1 | everything else |

`Style().layouts` is the one genuinely shared section (10 files) — it holds the view-level
`RatioLayout_t`s that every view resolves its panels against.

So this is not "thread a 36-member object through 268 sites". It is "**each element takes the one
struct it actually reads**", which is the design the guidelines ask for anyway: narrow, named
dependencies supplied at construction.

## Chosen design

1. **Leaves take their own section by reference.** A `UIElement` subclass gains a
   `const XStyle_t&` constructor parameter and stores it, exactly as `GrowthDisplay` now takes
   `const BaseDisplaySnapshot_t&`. Its `Style().x` reads become `m_rStyle.`.
2. **Views hold what their children need.** A view already builds its panels, so it takes the
   sections it passes down plus `LayoutsStyle_t` for its own `ResolveLayout` calls. Four sections
   is the worst case (`WorldView`).
3. **`ViewFactory` owns the `UiStyle`** and hands each view its slice — the same role it already
   plays for `GameState` and `GameDataContext`.
4. **`Engine` loads the style into an owned `UiStyle`** and passes it to `ViewFactory`, mirroring
   `LoadGameData` → `GameDataContext`.
5. **`Style()` and the `g_style` / `g_loaded` globals are deleted** once nothing calls them. Not
   kept as a bridge: a bridge that compiles is a bridge that gets used.

**`UiStyle` itself stays one aggregate struct** (renamed `UiStyle_t`, since it becomes a
config/POD bag with no behaviour). Splitting the *file* into per-feature headers is a separate
question and is **not** in this package — the god-object complaint is about the global and the
injection, not about how many structs share a header.

## Sequence

Each step compiles and passes on its own. Do not batch them.

1. **`UiStyle_t` + owned instance.** Add `Load` returning a value (`UiStyle_t Parse(path)`), keep
   the singleton alongside it. `Engine` and `ViewFixture` own an instance. No call sites change.
2. **Leaves, one directory at a time** — `ui/base`, `ui/unit-designer`, `ui/council`,
   `ui/commlinks`, `ui/satellite`, `ui/social-engineering`, `ui/settings`, `ui/research`,
   `ui/world`, then the shared `ui/*.cpp`. Each is a small, compiler-checked commit.
3. **Views**, in the same order, once their children take styles.
4. **`ViewFactory`**, then `Engine`.
5. **Delete `Style()`, `UiStyle::Get`, `UiStyle::Load` and the globals.** The commit that removes
   them is the one that proves nothing was missed.

## What must not regress

- **`ViewFixture` currently loads the shipped `config/ui/style.json` once per process** because it
  cannot inject. After step 1 it owns an instance; after the sweep, a test can build a themed
  `UiStyle_t` in-line and assert against it. That is the capability the finding is really about,
  so **land at least one test that injects a non-default style** and asserts the element drew with
  it — otherwise the package has moved code without buying anything.
- `tests/game/ConfigStrictnessTests.cpp` loads the real style file and mutates a colour to assert
  the parser rejects it. Keep both.
- Tests construct panels directly (`SatelliteButtonListPanel`, `SatelliteSummaryPanel`,
  `NoticePopup`). Those call sites gain a style argument; that is the point, not a cost.

## Risks, and the honest ones

- **This is a large behaviour-free diff, which is exactly the shape that hides silent errors.**
  The mitigations are that every step is compiler-checked (a missed conversion does not build, it
  does not misbehave), and that the UI now has tests. Unlike the `r`-prefix sweep package 16
  declined, a wrong edit here cannot compile.
- **`WorldView` is the outlier** (4 sections, 13 call sites) and `SocialEngineeringDisplay` is the
  densest single file (44 call sites, one section). Do the dense-but-simple one early to build
  confidence, and `WorldView` last.
- **Do not "improve" the sections while moving them.** No renames, no merges, no config changes
  in this package. The one exception already taken: `GrowthDisplayStyle_t` /
  `ProductionDisplayStyle_t` were collapsed into `ResourceLinesPanelStyle_t` beforehand, precisely
  so this package would not have to.

## Explicitly out of scope

- Splitting `UiStyle.h` into per-feature headers.
- Runtime theme switching or hot reload. The injection makes both possible; neither is asked for.
- The `[M]` "open/closed growth" complaint that a new panel needs a parser clone. Injection does
  not fix that — a registration table would, and it is a different change with a different risk
  profile. Record it as still open at the end of this package rather than smuggling it in.

## Analysis output path

`docs/full-review-fix-prompts/18-uistyle-injection.md` (this file — extend it with verified
diagnoses and a review-follow-ups section as the package runs, per the established pattern).
