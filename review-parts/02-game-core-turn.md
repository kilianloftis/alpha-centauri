## Game core — turn pipeline, hooks, and validators

**Files:** `src/game/TurnProcessor.cpp`, `include/game/TurnProcessor.h`,
`src/game/TurnStageFactory.cpp`, `include/game/TurnStageFactory.h`,
`src/game/TurnStageConfigParser.cpp`, `include/game/TurnStageConfigParser.h`,
`include/game/TurnStageRegistrar.h`, `include/game/TurnStages.h`,
`src/game/HookContext.cpp`, `include/game/HookContext.h`,
`src/game/EventBridge.cpp`, `include/game/EventBridge.h`,
`src/game/EffectReferenceValidator.cpp`, `include/game/EffectReferenceValidator.h`,
`src/game/RequiredTechValidator.cpp`, `include/game/RequiredTechValidator.h`

**Assessment:** The stage-type half of the extensibility story is genuinely good: prior finding
1.9's fix landed cleanly, the two narrow `Execute` interfaces mean no stage sees a parameter it
cannot use, `TurnStageRegistrar<T>` really does let a new built-in stage be added without editing
`TurnStageFactory.cpp`, and the `ac-turn-stages` OBJECT library correctly closes the static-init
drop hazard that design would otherwise carry. The dominant weakness is that the *config-driven*
half is unfinished in a way that silently changes behavior rather than failing: a declared
replace hook removes a stage's real work and substitutes nothing. Secondary to that,
`TurnProcessor` gained a yield/resume state machine with no exception story — any throw from a
stage leaves it wedged mid-stage with the hook lifecycle half-run.

### [H] A configured replace hook silently deletes the stage's behavior
`include/game/TurnStages.h:67-71` (and `:87-91`) skip `ExecuteImpl` entirely whenever
`HasReplaceHooks()` is true, and return `Continue`. But `HookContext::ExecuteReplaceHooks`
(`src/game/HookContext.cpp:50-60`) only runs `hook.callback`, and nothing in the repository ever
assigns `Hook_t::callback` — `TurnStageConfigParser::ParseHooks` (`src/game/TurnStageConfigParser.cpp:55-61`)
records `mod_id` and `script_path` and never loads the script. So the presence of a `replace`
entry in `turn_stages.json` is sufficient to turn a built-in stage into a no-op that prints a
line to stdout. `config/turn_stages.json` already ships such an entry (`CustomModStage`), and
adding one to `Upkeep` or `BaseProduction` would silently skip upkeep or production with no
error. This is the actionable core of the still-open prior finding 1.10: the fix is not "write
the Lua loader" but "stop gating `ExecuteImpl` on hook *presence*" — gate on a hook that can
actually run, and throw at config load when a hook names a `script_path` nothing can execute.
Related and worth fixing at the same time: `ExecuteReplaceHooks()` returns `void`
(`include/game/HookContext.h:29`), so a replace hook can never express `Yield` or failure — it
cannot substitute for the interface it replaces even once callbacks exist.

### [H] An exception from a stage wedges TurnProcessor and skips the post hooks
`EnsureEntered_` (`src/game/TurnProcessor.cpp:31-38`) sets `m_bStageEntered = true` before
`Execute` runs, and `CompleteStage_` (`:23-29`) is the only path that calls `OnExit()`. There is
no scope guard and no `try`. If a stage throws mid-turn — and stages reach code that throws
routinely, e.g. `PlayerActions` driving `UnitOrderExecutor` — post hooks never run, and
`m_bStageEntered` stays set, so the next `Advance` re-enters the *same* stage without running its
pre hooks, on top of half-applied turn state. `TurnStageBase`'s documented lifecycle (an `OnEnter`
is followed by an `OnExit`) is therefore only an invariant on the happy path. The same class of
problem applies to the processor's own throw at `:108`: it leaves `m_stageIndex == m_stageOrder.size()`
with `m_bStageEntered` false, so every subsequent `Advance` re-throws immediately without running
anything — the object is permanently poisoned with no `Reset`/abort entry point. Direction: wrap
the stage call so `OnExit` runs on unwind (or explicitly document and enforce "a throwing stage
aborts the turn"), and give the class a way to return to a known state.

### [M] `repeat_for_each_faction` is inert for every built-in stage, with no cross-check
`TurnStageFactory::CreateStageInstance` (`src/game/TurnStageFactory.cpp:81-92`) consults
`config.repeat_for_each_faction` only on the `Custom*` fallback path; for a registered id the
creator's C++ base class decides the shape and the flag is ignored. Every built-in entry in
`config/turn_stages.json` nonetheless carries the flag, so the file states a fact the engine does
not honor: setting `"repeat_for_each_faction": false` on `ResourceCollection` changes nothing.
`docs/architecture/turn-system.md:143` acknowledges the design, but nothing rejects a config that
contradicts the C++ type, which is exactly the desync a modder will hit first. Cheap fix: after
the bucketing in `CreateStages`, compare the resulting registry against `config.repeat_for_each_faction`
and throw on mismatch, making the flag either authoritative or verified.

### [M] `CreateStages` rediscovers the stage kind by RTTI and silently collapses duplicate ids
`src/game/TurnStageFactory.cpp:62-74` runs `DynamicUniquePtrCast<GlobalTurnStage>` then
`<PerFactionTurnStage>` on a `unique_ptr<TurnStageBase>` whose static type the registrar
(`include/game/TurnStageRegistrar.h:19-22`) already knew as `T`. This is part of prior finding
1.9's recorded fix, and it is the piece of that fix that will cost later: a third stage kind
means editing the cast chain, `TurnStageRegistries_t`, and `TurnProcessor::ExecuteCurrentStage_`.
`if constexpr (std::is_base_of_v<GlobalTurnStage, T>)` in the registrar would pick the bucket at
compile time and delete the RTTI step and the "neither" branch. Separately, both registry inserts
use `registries.global[config.id] = ...`, so two config entries sharing an `id` silently overwrite
— the second entry's hooks win, and since `Engine` builds the stage order 1:1 from the config
list, the *same* instance then runs twice per turn. Nothing anywhere rejects duplicate ids
(`JsonConfigLoader::LoadFile` does not check them). This also means a stage cannot legitimately
appear twice in a turn order with different hooks, which is a plausible mod request.

### [M] Per-faction resume relies on an ordering invariant no one enforces
`ExecutePerFactionStage_` (`src/game/TurnProcessor.cpp:59-72`) resumes by skipping every faction
whose id is `< *m_resumeFactionId`. `include/game/TurnProcessor.h:40-44` justifies this by
asserting that ids are monotonically allocated and `Factions()` stays in insertion order — true
today, but it is an invariant owned by `GameState`/`IdAllocator`, unenforced here, and the failure
mode if it ever breaks (turn-order shuffling, load-game reconstruction) is silent: factions are
skipped for a turn rather than anything throwing. Tracking a positional cursor or a set of
already-processed ids for the current stage would make the resume independent of id ordering.
The same comment reasons about the resume faction being "eliminated while yielded"; note that a
stage erasing a faction *during* the loop would invalidate the range-`for` outright, which the
comment does not mention. Nothing erases factions today, so that half is a latent trap, not a
live bug — but the comment currently gives more confidence than the code earns.

### [M] The effect validator's variant dispatch has no exhaustiveness guard
`ValidateEffectReferences` (`src/game/EffectReferenceValidator.cpp:52-74`) is an
`if/else if` chain of `std::get_if` over an 18-alternative `EffectVariant_t`. Adding a new effect
struct that carries a config id gets no compile-time reminder and simply goes unvalidated — the
exact "typo'd id becomes a silent no-op effect" failure this function exists to prevent. The
project already solves this elsewhere: `KindFor` in `include/game/effects/EffectEnums.h:92` is an
exhaustive switch backed by `-Werror=switch` in `src/CMakeLists.txt:138`, so adding a `StatId_t`
forces a decision. A `std::visit` with an overload set covering every alternative (with an
explicit no-op arm for the id-free ones) would give the validator the same property.

### [M] A missing registry silently disables validation instead of failing
`ValidateRequiredTechReferences` (`src/game/RequiredTechValidator.cpp:46-49`) returns early when
`rData.techRegistry` is null, turning the whole check into a no-op; the `GameDataContext` overload
of `ValidateEffectReferences` (`:131-134`) likewise passes raw `.get()` pointers whose null case
each check skips. `LoadGameData` always populates these, so the nullable path exists only for the
per-list test overload — but the guideline is to throw on an unexpected null, and here the
consequence of a wrong null is that every cross-config id check passes vacuously. Note
`tests/game/RequiredTechValidatorTests.cpp:85` currently encodes the silent skip as the intended
requirement, so this is a design decision to revisit rather than a plain bug: the
`GameDataContext` overloads should take the registries by reference and let the narrow, list-level
overload keep the nullable parameters the tests need. Related: both functions are hand-maintained
lists of `if (rData.X)` blocks over the same registries (`EffectReferenceValidator.cpp:142-205`,
`RequiredTechValidator.cpp:52-79`), so a new registry has to be remembered in three places
(`LoadGameData` plus both validators) and is silently unvalidated if it is not.

### [M] The turn-system architecture doc predates the yield/resume contract
`docs/architecture/turn-system.md:101-103,168` still documents `TurnProcessor::ProcessTurn(GameState&)`
as the entry point and describes it as a straight walk over `m_stageOrder`. That method no longer
exists; the real API is `Advance(GameState&)` with `StageResult_t::Yield`, mid-stage re-entry,
per-faction resume, and a throw when a full cycle produces no yield — none of which appear in the
doc at all. Since `architecting.md` makes keeping these diagrams current mandatory, and since the
yield contract is the single thing a new maintainer most needs before writing a stage, the doc is
now actively misleading. (The fix here lives in the doc rather than the source, but it is the
direct consequence of the change to `TurnProcessor.{h,cpp}`.)

### [M] EventBridge wiring stays opt-in, and now has more call sites to forget
`EventBridge::WireBase` (`src/game/EventBridge.cpp:15-24`) must be invoked by every code path that
creates a base, or that base emits no `EvBaseGainedPop`/`EvBaseLostPop` at all. This is open prior
finding 1.6, and it has gotten slightly worse rather than better: where the prior review recorded
one call site, `Engine` now has two (`Engine.cpp:188` and a `onBaseCreated` callback at `:452`),
so the pattern is spreading instead of being closed. The bridge is the right place to fix it —
subscribing to a faction-level base-created signal from inside `EventBridge` would make wiring a
property of the bridge instead of a step every future caller must remember. The lambdas also
capture `this` with no way to disconnect, which is safe only because `Engine` destroys
`m_eventBridge` after `m_pGameState`; that ordering is not stated anywhere near `WireBase`.

### [L] Convention and hygiene items
- `include/game/TurnStageConfigParser.h:18` — `bool repeat_for_each_faction;` breaks three rules at once: snake_case instead of camelCase, no `b` prefix, and no initializer (indeterminate on a default-constructed `TurnStageConfig_t`).
- `src/game/TurnStageConfigParser.cpp:41-61` — three byte-identical loops differing only in the JSON key and the `Add*Hook` call; one helper taking the key and a member-function pointer removes the triplication. The loop variable is named `hookId` but holds a hook object, not an id.
- `src/game/TurnStageConfigParser.cpp:44-45` — `value("mod_id", "")` / `value("script_path", "")` silently accept a hook entry with neither field, producing a hook that can never do anything; per the guidelines this should throw.
- `src/game/HookContext.cpp:30,42,54` — unconditional `std::cout` per hook, per stage, per turn; there is no logging facility in the project, so this is per-turn stdout spam once any hook is configured.
- `src/game/TurnStageFactory.cpp:60` — "Registered stage" is printed before the bucketing succeeds, so it also prints for a stage that immediately throws at `:72`.
- `src/game/TurnStageFactory.cpp:35-37`, `src/game/HookContext.cpp:7-9`, `src/game/TurnStageConfigParser.cpp:11-13` — empty user-declared constructors, plus matching `~X() = default` declarations (`TurnStageFactory.h:25`, `HookContext.h:21`, `TurnStageConfigParser.h:26`); all four are members added without a requirement.
- `include/game/TurnStages.h:56` — `HookContext m_hookContext` is `protected`, defeating the narrow `HasReplaceHooks`/`ExecuteReplaceHooks` accessors declared two lines above it; `CustomTurnStage.cpp:23` reaches for the member directly.
- `include/game/TurnStageFactory.h:38` — `CreateStageInstance` is a non-static member that touches no member state.
- `include/game/TurnStageConfigParser.h:3,7` — `game/TurnStages.h` and `<memory>` are included but unused by this header.
- `include/game/EffectReferenceValidator.h:29-31` — the doc comment lists the registries walked but omits factions, council proposals, council rules, and tile-yield rules, all of which the function does validate (`EffectReferenceValidator.cpp:187-205`). `include/game/RequiredTechValidator.h:9-10` has the same drift, omitting council proposals (`RequiredTechValidator.cpp:76-79`).
- `src/game/EffectReferenceValidator.cpp:34`, `src/game/RequiredTechValidator.cpp:28` — `ThrowBadReference_` / `ValidateRequiredTech_` use the trailing-underscore marker reserved for private methods on free functions in an anonymous namespace.

**Observed outside slice:**
- `docs/architecture/high-level.md:340,226` — refers to a `HookSystem` component that does not exist; `config/turn_stages.json` is loaded by `TurnStageFactory`.
- `docs/code-review-findings.md:178-182` — prior finding 1.8's status note assumes turn processing cannot yet pause mid-turn for player input; `StageResult_t::Yield` now implements exactly that, so the `HasOverlayView()` assertion it describes in `Engine::ProcessTurn_` needs re-examining against the yield path.
