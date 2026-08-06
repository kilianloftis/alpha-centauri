## Shared libraries

**Files:** `src/lib/EventBus.cpp`, `include/lib/EventBus.h`, `src/lib/LuaRuntime.cpp`, `include/lib/LuaRuntime.h`, `src/lib/Rational.cpp`, `include/lib/Rational.h`, `src/lib/config/ConfigFields.cpp`, `include/lib/config/ConfigFields.h`, `include/lib/Signal.h`, `include/lib/GameEvent.h`, `include/lib/IdAllocator.h`, `include/lib/RandomRoll.h`, `include/lib/DerefView.h`, `include/lib/Revision.h`, `include/lib/Registry.h`, `include/lib/config/JsonConfigLoader.h`

**Assessment:** The small utilities (`Revision`, `IdAllocator`, `RandomRoll`, `DerefView`, `JsonConfigLoader`, `ConfigFields` helpers) are clear and appropriately thin. `Signal` is in good shape after `Disconnect` / `ScopedConnection` / emit-time snapshotting, with solid unit coverage. The dominant weaknesses are the still-unsafe mod-facing `EventBus` dispatch path and `LuaRuntime::EvalInt`'s warn-and-zero / leaked-globals contract, both of which contradict project error-handling rules in the most extension-facing APIs.

### [H] Snapshot handlers in `EventBus::Publish` (reentrancy UB)
`src/lib/EventBus.cpp:23-26` — `Publish` iterates `m_handlers` directly. A handler that `Subscribe`s (vector reallocation) or `Unsubscribe`s (erase) during dispatch invalidates that iteration — undefined behavior on the bus documented as the mod-facing ABI (`docs/architecture/event-system.md`). `Signal::Emit` already snapshots and skips disconnected slots (`include/lib/Signal.h:107-117`); apply the same pattern here. This is the incomplete half of prior finding 1.6 (`Signal` side was fixed; `EventBus` was not). No `EventBus` reentrancy tests exist alongside `tests/lib/SignalTests.cpp`.

### [M] Make `LuaRuntime::EvalInt` fail loudly and isolate variables
`src/lib/LuaRuntime.cpp:37-70` — On any formula error the method logs to `stdout` and returns `0`; an empty formula returns `0` with no warning (`40-43`). Variables are written as persistent globals (`46-49`) and never cleared, despite the comment claiming they are "scoped to this call," so a missing input in formula B silently reads formula A's stale value. Callers use this for tech cost and pop composition (`TechCostCalculator.cpp:34`, `PopCompositionCalculator.cpp:29-30`), so typos become wrong game numbers instead of load/eval failures. Also, `static_cast<int>(result.get<lua_Number>())` (`63`) truncates non-integer results without complaint. Prefer throw-on-error (per guidelines and the 3.8 rule explicitly leaving this open under 3.6), evaluate in a fresh environment or nil-out keys after the call, and reject non-integral results. Prior finding 3.6 — still unresolved.

### [M] Keep `Registry::Load` atomic across validation failure
`include/lib/Registry.h:29-42` — `Load` replaces `m_configs` / `m_indexById` before `Validate_()`. If `ValidateNoDuplicates_` (or a subclass override) throws, the previous good registry contents are already gone and the object is left holding the rejected payload. Build the new index in locals, validate, then commit — or restore the prior vectors on failure — so a bad config file cannot destroy a previously loaded registry.

### [M] Reject wrong-typed arrays in `ParseStringArray`
`src/lib/config/ConfigFields.cpp:23-35` — When `key` is present but not a JSON array, the function returns `{}` with no error, same as a missing key. Mod authors who write `"prerequisites": "tech_x"` (or an object) get a silent empty list instead of a parse failure. Other loaders in this slice throw on shape errors (`JsonConfigLoader::LoadFile` at `include/lib/config/JsonConfigLoader.h:39-43`). If the key exists, require `is_array()` and throw otherwise.

### [M] Harden `Rational_t::ScaledInt` against overflow
`src/lib/Rational.cpp:59-73` — After reducing, the return is `num * (scale / den)` in `int`. Large numerators or scales invoke signed overflow (UB) before any divisibility check can help. Compute with a wider intermediate (e.g. `int64_t`) and throw if the product does not fit in `int`, matching the existing "must scale exactly" failure style.

### [L] Convention and hygiene items
- `include/lib/GameEvent.h:9-11` — `FactionId_t` / `BaseId_t` are re-declared here while `BaseTypes.h` also defines `FactionId_t`; `TechId` omits the `_t` suffix used by the sibling aliases (accepted layering tradeoff from prior 1.2, but the naming split remains).
- `include/lib/Registry.h:84-101` — `ValidateNoDuplicates_` is O(n²); collisions are already visible while filling `m_indexById` (prior §9 item, still true).
- `include/lib/LuaRuntime.h:25-26` / `src/lib/LuaRuntime.cpp:59,67` — Header documents warn-and-return-0; that contract itself fights the project throw-on-error rule once 3.6 is fixed.
- `include/lib/DerefView.h:16,22` — Transforms dereference `unique_ptr` with no null check; a null element is hard UB versus the guideline to throw on unexpected null.
- `include/lib/EventBus.h:34` vs `include/lib/Signal.h:130` — Subscription ids start at `0` on the bus and `1` on signals (`0` reserved); harmless but inconsistent.
- `include/lib/Signal.h` — Architecture doc still claims "zero heap allocation" for signals (`docs/architecture/event-system.md:61-62`) while the implementation stores `std::function` slots; update the doc when diagrams are next touched.

**Observed outside slice:**
- `docs/architecture/event-system.md` — Still describes snake_case signal names and "zero heap" `Signal` behavior that no longer matches `include/lib/Signal.h`.
- Prior 1.6 lifetime/wiring issues in `EventBridge` / `BaseManager` signal graphs remain caller-side; fix sites are outside this file list.
