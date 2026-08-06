# Package 3 — Object lifetime and ownership-transfer protocol

**Date:** 2026-08-06  
**Source:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md) Package 3; findings in [`docs/full-code-review.md`](../full-code-review.md) (Game core architecture; GameState / ownership transfer; EventBridge; UnitManager / DestroyUnit-as-transfer; Base / HomeBaseIndex; WorkerAssignmentManager displaced handler)  
**Verdict:** **Confirm** every listed finding (none fail to reproduce). **Amend** the fix direction: do **not** “complete the snapshot round-trip” as the primary transfer mechanism. `Unit` and `BaseManager` store `Faction&` (`Unit.h:159`, `BaseManager.h:220`), which cannot be rebound, and that is why transfer today is destroy-then-recreate — inheriting combat cargo rules, wrong events, and dangling `BaseView` refs. Prefer **identity-preserving ownership move** (release `unique_ptr`, rebind owner via pointer/`RebindFaction`, adopt) for transfer; keep **true destroy** (`DestroyUnit` / raze `ExtractBase`) for combat loss and base death. Write the protocol in architecture docs first; wire EventBridge and UI invalidation to that protocol. Do **not** split the package.

**Findings that no longer reproduce:** none. Line drift only: founding `WireBase` callback is `Engine.cpp:448` (review `:452`); Package 1–2 landed Yield / `CanAdvanceTurn` and updated `ui-system.md` modal text, but that doc’s BaseView/`BaseManager&` bullet still claims bases are never destroyed (`ui-system.md:246-249`) — still false.

**Packages 1–2 consumed:** Yield/`Advance` and modal turn-gating protect live `Pop&` / in-view `Unit*` during interactive stages. They do **not** cover base capture/raze/transfer or unit subversion mid-resolution. Package 3 owns that protocol.

---

## Verified diagnosis

### 1. No lifetime protocol while bases die and move — **confirmed [H]**

`ExtractBase` / `TransferBaseTo` are live call sites:

| Path | Locus |
|------|--------|
| Raze | `BaseConquestEffects.cpp:115` → `ExtractBase` |
| Capture | `BaseConquestEffects.cpp:307` → `TransferBaseTo` |
| Probe mind-control | `ProbeActionEffects.cpp:156` → `TransferBaseTo` |
| Diplomacy trade | `DiplomaticActionExecutor.cpp:328` → `TransferBaseTo` |

`TransferBaseTo` (`Faction.cpp:323-339`) extracts then `CreateBaseFromSnapshot` — new `BaseManager` address, same `baseId`. `ViewFactory::CreateBaseView` still takes `BaseManager&` (`ViewFactory.h:51-56`); `BaseView` stores it for life (`BaseView.h:42`) with no pop-on-destroy / owner-change path.

`docs/architecture/ui-system.md:246-249` still says no code destroys a `BaseManager` — contradicted by the table above. `docs/architecture/turn-system.md:151` defers mid-stage faction erase “until the lifetime protocol defines it.” `high-level.md` has no destroy/transfer protocol section.

### 2. Ownership transfer is destroy-then-recreate — **confirmed [H]**

`Faction::ExtractUnit` (`Faction.cpp:342-359`) captures `UnitSnapshot_t` then calls `UnitManager::DestroyUnit`. Snapshot (`Unit.h:25-34`) has no carrier/cargo. `DestroyUnit` (`UnitManager.cpp:71-118`) applies carrier-loss cargo rules and emits `OnUnitDestroyed` before erase. `GameState::AddFaction` connects that signal to revealed-unit cleanup (`GameState.cpp:206-212`) and `OnUnitCreated` to first-contact (`:213-217`) — so subvert/transfer looks like death then birth to every observer.

Failure is non-atomic: `TransferUnitTo` / `TransferBaseTo` (`Faction.cpp:383-397`, `:322-339`) have already extracted when `Create*FromSnapshot` throws; no rollback.

Probe subvert uses this path (`ProbeActionEffects.cpp:180-181`).

### 3. `DestroyUnit` is the transfer path (combat cargo rules) — **confirmed [H]**

Same `DestroyUnit` body: cargo that cannot hold the tile is destroyed; survivors `Disembark` (`UnitManager.cpp:77-93`). Transferring a loaded transport therefore sinks or strands passengers; the receiver gets an empty hull.

Embarked-unit transfer: `DestroyUnit` → destructor/`DetachFromWorld_` clears carrier links (`Unit.cpp:79-110`); `CreateUnitFromSnapshot` → `CreateUnit` → `CanPlaceUnitOnTile` (`UnitManager.cpp:54-58`) against a tile the carrier still occupies — under single-unit-per-tile this throws **after** the source unit is gone.

Transport tests correctly pin **combat** carrier-loss (`TransportTests.cpp` “destroying a carrier over water drowns its cargo”) — that requirement must stay on `DestroyUnit` only.

### 4. Building deploy cooldowns leak across transfer — **confirmed [M]**

`DeployBuilding` appends forever (`Faction.cpp:193-196`); nothing prunes on expiry. `CountReadyBuildings` rescans all records (`:179-191`). `NotifyBuildingDestroyed` (`:198-209`) matches `buildingId` only — comment claims “prefer still cooling,” but any matching record (often expired) is erased. `ExtractBase` / `TransferBaseTo` never touch `m_buildingDeploys` (`Faction.h:278-284`), so losing an ODP-bearing base leaves a phantom cooldown that suppresses a later rebuild.

### 5. EventBridge wiring is opt-in — **confirmed [M]**

`EventBridge::WireBase` (`EventBridge.cpp:15-24`) connects pop signals only when called. Call sites today:

- Startup bases: `Engine.cpp:192`
- Colony-pod founding callback: `Engine.cpp:448` (via `UnitOrderExecutor` `onBaseCreated`)

`CreateBaseFromSnapshot` → `AddBase` (`Faction.cpp:235-248`, `:318-320`) does **not** wire. Capture / mind-control / trade therefore produce bases with silent mod-facing pop events. Faction-level signal TODO remains (`EventBridge.cpp:11-12`). Lambdas capture `this` with lifetime tied to Engine destroying the bridge after `GameState` — ordering undocumented at `WireBase`.

### 6. Displaced-worker handler outlives `WorkerAssignmentManager` — **confirmed [M]** (latent)

`Assign_` stores `[this]{ AutoAssignWorkers(); }` in the claim (`WorkerAssignmentManager.cpp:92-93`). `WorkedTileIndex.h:13-16` requires the handler valid for the claim’s life. `BaseManager` constructs `m_pPopulation` before `m_pWorkerAssignments` (`BaseManager.cpp:76-81`), so destruction order destroys the manager **first** while pops (and claims) still exist. `~WorkerAssignmentManager` is `= default` (`.h:30`). Latent: only `ClaimDisplacing` invokes the handler; ordinary pop teardown does not — still a use-after-free trap if displacement runs during teardown.

### 7. `HomeBaseIndex` triple representation — **confirmed [M]**

Parallel `m_claims` / `m_units` plus `HomeBaseClaim::m_pUnit` (`HomeBaseIndex.h:41-42,78-79`). `m_pUnit` is written in the claim ctor/move/release (`HomeBaseIndex.cpp:12,46,52,62,68`) and **never read** — `GetBase()` uses `m_pIndex` only (`:38-40`). Alignment depends on erase arithmetic at `:102-104`.

### 8. `HomeBaseIndex` throws from `noexcept` paths — **confirmed [M]** (contract / latent)

`Release_` throws `logic_error` (`:100`) from `~HomeBaseClaim` (implicitly `noexcept`). `UpdateClaimPointer_` throws (`:113`) from `noexcept` move ctor/assign (`HomeBaseIndex.h:25-26`). Either is `std::terminate`. No live call sequence found today (claims only from `Claim` + NRVO); still a false exception contract.

---

## Chosen design

### Normative lifetime protocol (write into `docs/architecture/high-level.md`, cross-link `ui-system.md` / `faction-system.md` / `turn-system.md`)

1. **Destroy (unit):** Only `UnitManager::DestroyUnit`. Applies combat carrier-loss cargo rules; emits `OnUnitDestroyed` **before** the object is erased/deferred; UI and `GameState` may invalidate addresses.
2. **Transfer (unit):** Must **not** call `DestroyUnit`. Release ownership without cargo-loss rules and without `OnUnitDestroyed`. Preserve cargo graph when the transferred unit is a carrier (passengers move with it to the new faction). Preserve embarkation when transferring a passenger (stay on the same carrier if the carrier’s faction still matches after the operation’s rules — default for subvert: transfer the targeted unit only; if embarked, detach cleanly **without** destroying, then place/adopt under the new owner without requiring the carrier’s tile to be empty for a recreate). Emit an explicit transferred/adopted signal (or only `OnUnitCreated` on the receiver **without** a prior destroy on the giver — prefer a dedicated `OnUnitTransferred` / adopt path so mods do not see fake death).
3. **Destroy (base):** Raze / explicit remove via extract-and-destroy (today `ExtractBase`). Object dies; home-base claims orphan (already); deploy records for that `baseId` drop; UI must close any `BaseView` for that id/address; EventBridge edges die with the object.
4. **Transfer (base):** **Identity-preserving ownership move** — `unique_ptr<BaseManager>` leaves giver, `RebindFaction` (see below), `AddBase` on receiver. Same address and `baseId`. **Not** destroy+`CreateBaseFromSnapshot`. Observers see owner change / base-list change, not destroyed+created. Recalculate composition / worker assignment under new owner (psych may differ) as today after snapshot recreate (`Faction.cpp:313-316`), but without rebuilding the `BaseManager`.
5. **Who may destroy/transfer:** Gameplay effects and diplomancy executors (conquest, probes, trade) call `Faction` / `UnitManager` APIs only — not ad-hoc `erase` on vectors. UI never destroys.
6. **UI rule:** Views may hold `BaseManager&` / `Unit*` only while the protocol guarantees validity **or** they subscribe to destroy/transfer signals and clear/pop. Concrete minimum: open `BaseView` pops when that base is destroyed **or** its owning faction is no longer the faction the view was opened for (after Package 15 drops the dual `Faction&`, “owner changed” is enough). `WorldView` already clears selection on `OnUnitDestroyed`; it must also clear on unit transfer-away (address may survive if identity-preserved — selection may remain valid if the unit stayed; if the unit left the player faction, clear or keep as foreign per UX — **requirement: selection must not dangle**; if identity preserved and unit still exists, pointer stays valid).
7. **EventBridge:** Wiring is a property of “base exists in the session,” not a per-call-site chore. Wire exactly once when a base object is first introduced (founding / load). Identity-preserving transfer keeps existing wires. Do not require conquest/diplomacy callers to call `WireBase`.
8. **Deploy cooldowns:** Per-copy (or per-base) state must not outlive the building copy’s ownership. Key records with `baseId` (and building id); move or drop them in base release/transfer; prune expired entries in `CountReadyBuildings` or on turn boundary; `NotifyBuildingDestroyed` must prefer a still-cooling record for that base when mission year is known (pass year or store only active).
9. **Faction erase mid-turn:** Still unsupported in this package (as `turn-system.md` states). Protocol documents the gap; do not implement faction elimination here.
10. **Modal / Yield interaction (Packages 1–2):** Do not run player-facing destructive transfer/raze while a modal holds live refs into the dying object if that path is UI-initiated; combat/probe resolution that mutates under engine control remains valid and must invalidate UI via signals.

### Implementation sketch

**Rebindable owner**

- Change `Unit::m_rFaction` and `BaseManager::m_rFaction` from `Faction&` to `Faction*` (or a small `OwnerRef` that supports `Rebind`) set at construction and updated only by transfer adopt. Public `GetFaction()` still returns `Faction&` (throw if null — should never be null while in a manager).
- This is the enabling change for identity-preserving transfer; snapshot recreate cannot fix `BaseView`’s live reference without also adding handles.

**UnitManager**

- Add `ReleaseUnit(Unit&) -> std::unique_ptr<Unit>` (name flexible): remove from `m_units`, bump revision, rebuild visibility as needed; **do not** run carrier-loss loop; **do not** emit `OnUnitDestroyed`; **do not** call `DetachFromWorld_` (unit stays on map). Clear or preserve cargo per transfer rules above.
- `DestroyUnit` unchanged in combat semantics (keep TransportTests).
- `AdoptUnit(std::unique_ptr<Unit>, Faction&)` / faction helper: rebind owner, push into `m_units`, emit created/transferred, visibility/first-contact as appropriate without a fake destroy.
- `Faction::TransferUnitTo` uses Release+Adopt (and carrier cargo group rules). Remove `ExtractUnit`→`DestroyUnit` coupling. Keep snapshot APIs only if still needed for save/tests; they must not be the live transfer path.

**Faction bases**

- `ReleaseBase(BaseId_t) -> std::unique_ptr<BaseManager>` for move-out (list bump, territory, visibility).
- `TransferBaseTo`: ReleaseBase → `RebindFaction` → receiver `AddBase`; then composition/worker refresh; migrate deploy records for that `baseId`.
- Raze continues to destroy the released object (or `ExtractBase` becomes destroy-only and stops being used for transfer).
- `CreateBaseFromSnapshot` may remain for future load; must go through the same “new base introduced” wiring hook as `CreateBase`.

**EventBridge**

- Subscribe once at composition (Engine or EventBridge ctor) to a faction-level / GameState-level “base added” signal that `AddBase` emits for every insertion — including founding and post-transfer adopt. `WireBase` idempotent (guard against double-connect) or track wired base ids.
- Remove the need for the duplicate Engine founding callback as the *only* wire path (callback may remain for open-BaseView UX, but wiring must not depend on it).

**UI**

- On base destroyed or ownership change: if active overlay is `BaseView` for that base, mark close / pop.
- Update `ui-system.md` Object Lifetime: delete “bases never destroyed”; document destroy vs transfer and pop rules. Consume Package 2 `CanAdvanceTurn` text already there.

**HomeBaseIndex / workers**

- Delete `HomeBaseClaim::m_pUnit`; keep `m_units` + `m_claims`.
- Replace throws in `Release_` / `UpdateClaimPointer_` with `assert` / `AC_ASSERT` (or `std::abort` with message) — no exception from `noexcept` paths.
- `~WorkerAssignmentManager`: clear every pop’s `WorkedTileClaim` before the manager finishes destroying.

### Rejected alternatives

| Alternative | Why not |
|-------------|---------|
| Complete snapshot round-trip (carrier/cargo in `UnitSnapshot_t`) as primary transfer | Still destroys addresses; still fights `Faction&`; easy to get embarked/`CanPlaceUnitOnTile` wrong; mods still see destroy+create unless signals are specially suppressed. Keep snapshots for save if needed, not for live transfer. |
| Keep destroy-then-recreate; only add UI `BaseId` weak lookup | Fixes dangling view if every holder converts to id+lookup, but leaves cargo/combat-on-transfer and fake events; fights the brief’s “base changed owner ≠ destroyed.” |
| Weak handles / `shared_ptr` for all bases/units | Heavier than needed; RAII `unique_ptr` ownership in `Faction`/`UnitManager` is the model. Signals + pop is enough for UI. |
| Fix only probe/diplomacy call sites with ad-hoc cargo handling | Leaves the API footgun; next caller will reuse `ExtractUnit`. |
| Split HomeBaseIndex / deploy / EventBridge into a follow-up package | Same protocol; splitting re-negotiates “what transfer means” twice. |
| Faction elimination / mid-turn faction erase | Explicitly deferred (`turn-system.md`); out of scope. |
| Production typed-handle / stockpile snapshot completeness | Package 6; identity-preserving base transfer largely avoids the recreate bug for transfer. |
| Secret-project sabotage / probe notify / combat outcome correctness | Package 8. |
| Remove `BaseView`’s dual `Faction&` | Package 15; Package 3 only needs pop-on-destroy/transfer. |
| Full mod-seam / Lua hook / EventBus snapshot publish | Broader “Shape mod seams” finding — not in Package 3 table; only close WireBase opt-in. |

---

## Interactions with other packages

| Package | Interaction |
|---------|-------------|
| **1** Turn pipeline | Consumed. Do not change Yield/`Advance`. Faction erase mid-stage still deferred. |
| **2** Modal / turn gate | Consumed. Modal gate protects UI-held `Pop&`; Package 3 adds destroy/transfer invalidation for bases/units. Do not redefine `CanAdvanceTurn`. |
| **4** Composition root | EventBridge/Engine wiring cleanup should stay compatible with future session phases; avoid new late `Set*` if an `AddBase` signal suffices. |
| **6** Base economy | Production validation / typed production handle / composition-on-`RemovePop` stay in 6. Identity-preserving transfer reduces pressure on `CreateBaseFromSnapshot` production id bugs for capture. |
| **8** Combat / probes / conquest | Call sites already use `Transfer*To` / `DestroyUnit`. Package 3 changes transfer semantics underneath; Package 8 must not reintroduce Destroy-as-transfer. Sabotage/`NotifyBuildingDestroyed` gaps remain 8. |
| **9** Buildings / SP | Tombstone on raze stays; deploy ledger fix here must not break intercept/orbital deploy accounting. |
| **15** UI view correctness | Dual `Faction&` on `BaseView` is 15. Package 3 owns pop-on-destroy/owner-change. |
| **16** Docs hygiene | Package 3 updates lifetime sections in `high-level.md` / `ui-system.md` (and brief cross-links). Broader doc sweep stays 16. |
| **17** Diplomacy / visibility | Trade base path uses `TransferBaseTo`; visibility/`OnUnitDestroyed` fake-death on subvert is fixed by transfer signals — do not redesign first-contact here beyond not lying about destroy. |

---

## Implementation plan

1. **Docs first (protocol)**  
   - Add an “Object lifetime and ownership transfer” section to `docs/architecture/high-level.md` with the normative rules above.  
   - Fix `ui-system.md` deferred BaseView bullet.  
   - One-line cross-link from `turn-system.md` / `faction-system.md` as needed.

2. **Rebindable faction owner**  
   - `Unit` and `BaseManager`: store rebinding owner; `RebindFaction(Faction&)`; `GetFaction()` unchanged in spirit.

3. **Unit release / adopt**  
   - `UnitManager::ReleaseUnit` vs `DestroyUnit`; `Faction::TransferUnitTo` rewritten; cargo-with-carrier preservation; embarked subvert without tile-conflict loss.  
   - Adjust `GameState` signal expectations (no Forget-on-fake-destroy for transfer).

4. **Base release / transfer / raze**  
   - Split transfer move from raze destroy; update `BaseConquestEffects`, `ProbeActionEffects`, `DiplomaticActionExecutor` only if signatures change (prefer keeping `TransferBaseTo` / `ExtractBase` names with new semantics).

5. **Deploy ledger**  
   - Key by `baseId` + `buildingId`; migrate/drop on base release; prune expired; fix `NotifyBuildingDestroyed` preference.

6. **EventBridge**  
   - Auto-wire on base introduction; idempotent `WireBase`; collapse reliance on founding-only callback.

7. **UI invalidation**  
   - Pop `BaseView` on destroy/owner-change; ensure `WorldView` selection cannot dangle across transfer-away.

8. **HomeBaseIndex + WorkerAssignmentManager**  
   - Remove dead `m_pUnit`; assert-not-throw on invariant paths; clear claims in `~WorkerAssignmentManager`.

9. **Tests** (below) via `./bd test`.

---

## Test plan

Requirement-based (assert protocol rules, not today’s destroy-then-recreate behaviour):

1. **Transfer unit ≠ destroy**  
   Subvert/transfer a unit: giver loses it, receiver owns same `unitId` (and, if identity-preserving, same address); `OnUnitDestroyed` must **not** fire for that transfer; combat `DestroyUnit` still fires `OnUnitDestroyed` (existing `UnitPositionTests` signal case remains).

2. **Transfer loaded transport preserves cargo**  
   Carrier with embarkable passenger transferred: both end under receiver; passenger still embarked (or explicitly documented disembark-without-death if protocol chooses — default **preserve embarkation**); no drowning on land/water solely due to transfer.

3. **Destroy carrier still applies cargo rules**  
   Existing TransportTests carrier-loss cases remain green (requirement unchanged).

4. **Transfer embarked unit does not lose the unit**  
   Subvert/transfer an embarked passenger while carrier remains: no throw; unit exists under receiver; carrier’s cargo list consistent (link cleared or updated per protocol).

5. **Transfer base preserves identity**  
   `TransferBaseTo`: same `BaseManager*` address and `baseId` under receiver; giver `GetBaseCount` decreased; `GetFaction()` on the base is the receiver.

6. **Raze destroys**  
   Existing raze/tombstone conquest test remains: base gone after raze; Secret Project tombstoned.

7. **Deploy cooldown does not leak**  
   Deploy building on base A; transfer or extract A; giver’s `CountReadyBuildings` for that id is not suppressed by A’s old cooldown; expired records do not accumulate unbounded across many deploy cycles (prune observable).

8. **EventBridge after transfer**  
   Base introduced only via transfer/capture path still publishes pop events on `EventBus` after `OnPopGained`/`OnPopLost` (wire without Engine founding callback).

9. **UI / base view invalidation**  
   If testable without full SFML: destroying or transferring away a base that has an open BaseView causes should-close / pop. At minimum, a unit/base list observer test proves the signal fires; wire UI to it.

10. **HomeBaseIndex**  
    Existing home-base tests stay green; claim move/destroy still orphans correctly; no public API depends on removed `m_pUnit`.

11. **WorkerAssignmentManager teardown**  
    Destroy a base with assigned workers (extract/raze): no crash under ASan; claims cleared (can assert worked-tile index has no handlers pointing at freed manager — or simply ASan-clean teardown + displacement during manager life still auto-assigns).

**Existing tests that pinned bugs / outdated contracts and must change:**

| Test / doc | Why |
|------------|-----|
| Any test that assumes transfer emits `OnUnitDestroyed` then `OnUnitCreated` | Requirement change: transfer is not destruction. Update to assert transferred/adopt semantics. |
| Any test that assumes `TransferBaseTo` yields a new `BaseManager*` | Requirement change: identity preserved. |
| `docs/architecture/ui-system.md` “No code destroys a BaseManager” | Doc false today; rewrite for protocol. |
| `HomeBaseTests` “Destroying a base orphans…” via `ExtractBase` | Keep for **raze/destroy**; add a separate transfer case that does **not** orphan solely due to owner change (home claims: units of the old faction — define: foreign home claims invalid; home units of the transferred base’s garrison may need re-home — state in protocol: home claims to the moved base remain valid because the `HomeBaseIndex` moves with the `BaseManager`). |
| Transport carrier-loss tests | **Must not** be weakened; they pin `DestroyUnit` combat rules. |

Do **not** weaken Package 1–2 turn/modal tests.

---

## AI implementation prompt

```markdown
# Implement Package 3 — Object lifetime and ownership-transfer protocol

You are working in the Alpha Centauri C++ rebuild at `/home/martok/alpha-centauri`.

## Goals

1. **Document one lifetime protocol** in `docs/architecture/high-level.md` (cross-link `ui-system.md` / faction / turn docs as needed):
   - **Destroy unit** = `DestroyUnit` (combat cargo rules + `OnUnitDestroyed`).
   - **Transfer unit** ≠ destroy: release/adopt with owner rebind; preserve carrier cargo on peaceful transfer; no fake destroy events.
   - **Destroy base** = raze/extract-destroy; UI and deploy ledger invalidate.
   - **Transfer base** = identity-preserving ownership move (same `BaseManager` object), not snapshot destroy/recreate.
   - EventBridge wiring is automatic on base introduction; UI pops BaseView on destroy or owner change.
   - Faction elimination mid-turn remains out of scope.

2. **Enable rebindable ownership** on `Unit` and `BaseManager` (replace non-rebindable `Faction&` members with a pointer/`RebindFaction` path). `GetFaction()` continues to return a live `Faction&` for valid objects.

3. **Split `UnitManager` release from destroy.** `Faction::TransferUnitTo` / probe subvert must not apply carrier-loss rules or emit `OnUnitDestroyed`. Keep `DestroyUnit` behaviour for real deaths (TransportTests stay green).

4. **Split base transfer from base destroy.** `TransferBaseTo` moves `unique_ptr<BaseManager>` and rebinds faction; raze remains destructive. Update conquest / probe / diplomacy callers only as needed to keep using the Faction APIs.

5. **Deploy cooldowns:** stop leaking across base transfer; key by base; prune expiry; make `NotifyBuildingDestroyed` match its comment (prefer still-cooling record for that base).

6. **EventBridge:** wire pop events from a single base-introduction path (`AddBase` / signal), idempotent, so capture/trade/founding all publish. Do not leave wiring as a call-site chore on Engine founding only.

7. **UI:** pop/close BaseView when its base is destroyed or changes owner; ensure WorldView selection cannot dangle across transfer-away. Update `ui-system.md` lifetime section (remove “bases never destroyed”).

8. **HomeBaseIndex:** remove unread `HomeBaseClaim::m_pUnit`; stop throwing from `noexcept` paths (assert/abort). **WorkerAssignmentManager:** destructor clears all pop tile claims.

## Constraints

- Follow `.cursor/rules/coding-guidelines.md`: SOLID, references over pointers at APIs where practical, throw on unexpected nulls, no legacy shims that preserve destroy-as-transfer.
- Build and test **only** via `./bd` (never raw cmake/make/ctest).
- Read and follow: `docs/full-review-fix-prompts/03-lifetime-and-transfer.md` (verified diagnosis + design). Findings: `docs/full-code-review.md`. Brief: `docs/full-review-fix-packages.md` Package 3.
- Packages 1–2 have landed (Yield/`Advance`, `CanAdvanceTurn`, modal contract). **Consume them; do not redefine.**
- Prefer identity-preserving transfer over “better snapshots.” Snapshots may remain for save/load later but must not be the live transfer implementation.

## Analysis reference

`docs/full-review-fix-prompts/03-lifetime-and-transfer.md`

## Primary files

- `docs/architecture/high-level.md`, `docs/architecture/ui-system.md`
- `include/game/GameState.h`, `src/game/GameState.cpp`
- `include/game/Faction.h`, `src/game/Faction.cpp`
- `include/game/faction/UnitManager.h`, `src/game/faction/UnitManager.cpp`
- `include/game/units/Unit.h`, `src/game/units/Unit.cpp`
- `include/game/faction/base/BaseManager.h`, `src/game/faction/base/BaseManager.cpp`
- `src/game/units/BaseConquestEffects.cpp`, `src/game/units/ProbeActionEffects.cpp`
- `src/game/faction/DiplomaticActionExecutor.cpp`
- `src/game/EventBridge.cpp`, `include/game/EventBridge.h`, `src/game/Engine.cpp`
- `include/ui/ViewFactory.h`, `include/ui/base/BaseView.h`, `src/ui/base/BaseView.cpp`, `src/ui/world/WorldView.cpp`
- `include/game/faction/base/HomeBaseIndex.h`, `src/game/faction/base/HomeBaseIndex.cpp`
- `include/game/faction/base/resources/WorkerAssignmentManager.h`, `src/game/faction/base/resources/WorkerAssignmentManager.cpp`
- Tests: `tests/game/TransportTests.cpp` (must stay), `tests/game/HomeBaseTests.cpp`, `tests/game/BaseConquestTests.cpp`, `tests/game/ProbeActionTests.cpp` / new transfer-focused cases, `tests/game/UnitPositionTests.cpp` signal expectations, deploy/EventBridge coverage as needed

## Acceptance criteria

- [ ] Architecture docs state destroy vs transfer for bases and units; `ui-system.md` no longer claims bases are immortal.
- [ ] `TransferUnitTo` does not call `DestroyUnit`, does not drown cargo, does not emit `OnUnitDestroyed` for the transferred unit.
- [ ] `DestroyUnit` still applies carrier-loss cargo rules; TransportTests pass.
- [ ] Embarked-unit transfer cannot destroy the unit via `CanPlaceUnitOnTile` throw-after-extract.
- [ ] `TransferBaseTo` preserves `BaseManager` object identity and rebinds owner; raze still removes the base.
- [ ] Deploy cooldowns do not suppress buildings after the originating base left the faction; ledger does not grow forever without prune.
- [ ] Bases created via capture/trade emit EventBus pop events without a special Engine-only wire call.
- [ ] BaseView closes (or equivalent) when its base is destroyed or ownership changes; no dangling `BaseManager&`.
- [ ] `HomeBaseClaim` has no dead `m_pUnit`; invariant failures are not exceptions from `noexcept`.
- [ ] `WorkerAssignmentManager` teardown clears claims (ASan-clean base destroy with workers).
- [ ] Requirement-based tests for the above; `./bd test` passes for affected suites.
- [ ] Package 1–2 turn/modal behaviour unchanged.

## Out of scope

- Faction elimination / erasing a faction mid-stage (document only).
- Production typed handles, SetProduction validation, pop composition dirty-on-RemovePop (Package 6).
- Probe sabotage Secret Project tombstone, `NotifyBuildingDestroyed` on sabotage, combat math (Package 8).
- Removing BaseView’s dual `Faction&` constructor param (Package 15) except as needed for pop-on-transfer.
- Full mod Lua hooks, EventBus publish snapshotting, sample mod (broader mod-seam work).
- Package 4 composition-root session split beyond what EventBridge auto-wire needs.

## What NOT to do

- Do not keep `ExtractUnit` → `DestroyUnit` as the transfer implementation.
- Do not “fix” transfer by only expanding `UnitSnapshot_t` while still destroying addresses and firing destroy events.
- Do not weaken TransportTests carrier-loss assertions.
- Do not require every conquest/diplomacy caller to remember `EventBridge::WireBase`.
- Do not leave deploy records keyed only by building id with no base association.
- Do not throw `logic_error` from `HomeBaseClaim` destructor / noexcept moves.
- Do not redefine Yield, `CanAdvanceTurn`, or modal routing (Packages 1–2).
- Do not implement faction wipe / mid-turn faction erase in this package.
- Do not expand into Package 6/8/15/17 feature work beyond what the protocol APIs require.
```
