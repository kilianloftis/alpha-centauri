// Package 3 — object lifetime and ownership-transfer protocol.
//
// Destroy (unit) = UnitManager::DestroyUnit only: combat carrier-loss cargo rules,
// OnUnitDestroyed. Transfer (unit) = Faction::TransferUnitTo, built on
// UnitManager::ReleaseUnit + AdoptUnit: no cargo loss, no OnUnitDestroyed, identity
// preserved. Destroy (base) = Faction::ExtractBase: object dies, OnDestroyed fires, deploy
// ledger for that base drops. Transfer (base) = Faction::TransferBaseTo: identity-preserving
// unique_ptr move + RebindFaction, deploy ledger migrates. See
// docs/architecture/high-level.md, "Object lifetime and ownership transfer".

#include "GameFixtures.h"

#include "game/EventBridge.h"
#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/map/WorkedTileIndex.h"
#include "game/population/pop-types/Pop.h"
#include "game/units/Unit.h"
#include "lib/EventBus.h"
#include "lib/GameEvent.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ac;
using namespace actest;

namespace
{

Pop& FirstWorkerPop_(BaseManager& rBase)
{
    for (Pop& rPop : rBase.GetPopulation().Pops())
    {
        if (rPop.IsWorker())
        {
            return rPop;
        }
    }
    throw std::logic_error("base has no worker pop");
}

} // namespace

// --- Unit transfer -----------------------------------------------------------------------

TEST_CASE("TransferUnitTo does not destroy: identity preserved, no OnUnitDestroyed",
          "[unit][lifetime][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    Unit& unit = fixture.MakeUnit(giver, 4, 4, {"test_chassis"});
    Unit* pAddress = &unit;
    const UnitId_t unitId = unit.GetUnitId();

    bool bDestroyedFired = false;
    bool bReleasedFired = false;
    bool bAdoptedFired = false;
    giver.GetUnitManager().OnUnitDestroyed.Connect([&](Unit&) { bDestroyedFired = true; });
    giver.GetUnitManager().OnUnitReleased.Connect([&](Unit& rReleased) {
        bReleasedFired = true;
        CHECK(&rReleased == pAddress);
    });
    receiver.GetUnitManager().OnUnitAdopted.Connect([&](Unit& rAdopted) {
        bAdoptedFired = true;
        CHECK(&rAdopted == pAddress);
    });

    giver.TransferUnitTo(unitId, receiver);

    CHECK_FALSE(bDestroyedFired);
    CHECK(bReleasedFired);
    CHECK(bAdoptedFired);

    // Same object, same id, now under the receiver.
    bool bFoundUnderReceiver = false;
    for (Unit& rUnit : receiver.GetUnitManager().Units())
    {
        if (&rUnit == pAddress)
        {
            bFoundUnderReceiver = true;
            CHECK(rUnit.GetUnitId() == unitId);
        }
    }
    CHECK(bFoundUnderReceiver);
    CHECK(&pAddress->GetFaction() == &receiver);

    // Gone from the giver.
    for (Unit& rUnit : giver.GetUnitManager().Units())
    {
        CHECK(&rUnit != pAddress);
    }
}

TEST_CASE("TransferUnitTo clears the produced-at record, not just the home claim",
          "[unit][lifetime][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    BaseManager& giverBase = fixture.MakeFactionBase(giver, 2, 2);
    Unit& unit = fixture.MakeUnit(giver, 3, 2, {"test_chassis"}, &giverBase, &giverBase);
    Unit* pAddress = &unit;
    REQUIRE(unit.GetProducedAtBase() == &giverBase);
    REQUIRE(unit.GetHomeBase() == &giverBase);

    giver.TransferUnitTo(unit.GetUnitId(), receiver);

    CHECK(pAddress->GetHomeBase() == nullptr);
    CHECK(pAddress->GetProducedAtBase() == nullptr);

    // The record must be cleared, not merely unresolvable: GetProducedAtBase looks the id up
    // among the unit's *own* faction's bases, so a surviving id would come back to life the
    // moment the receiver captured that base and let the unit claim ProducedAtThisBase bonuses
    // for a base it was never built in.
    giver.TransferBaseTo(giverBase.GetBaseId(), receiver);
    CHECK(pAddress->GetProducedAtBase() == nullptr);
}

TEST_CASE("Transfer to self is rejected rather than corrupting ownership",
          "[unit][base][lifetime][transfer]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    Unit& unit = fixture.MakeUnit(faction, 3, 2, {"test_chassis"});

    CHECK_THROWS(faction.TransferUnitTo(unit.GetUnitId(), faction));
    CHECK_THROWS(faction.TransferBaseTo(base.GetBaseId(), faction));

    // Both are still owned and intact after the refusal.
    CHECK(&unit.GetFaction() == &faction);
    CHECK(&base.GetFaction() == &faction);
    CHECK(faction.GetBaseCount() == 1);
}

TEST_CASE("Transferring a unit or base that does not exist throws",
          "[unit][base][lifetime][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    CHECK_THROWS(giver.TransferUnitTo(/*unitId*/ 9999, receiver));
    CHECK_THROWS(giver.TransferBaseTo(/*baseId*/ 9999, receiver));
    CHECK_THROWS(giver.ReleaseBase(/*baseId*/ 9999));
}

TEST_CASE("DestroyUnit still applies combat rules and fires OnUnitDestroyed for real death",
          "[unit][lifetime][destroy]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    bool bDestroyedFired = false;
    bool bReleasedFired = false;
    faction.GetUnitManager().OnUnitDestroyed.Connect([&](Unit&) { bDestroyedFired = true; });
    faction.GetUnitManager().OnUnitReleased.Connect([&](Unit&) { bReleasedFired = true; });

    faction.GetUnitManager().DestroyUnit(unit);

    CHECK(bDestroyedFired);
    CHECK_FALSE(bReleasedFired);
}

TEST_CASE("Transferring a loaded transport preserves cargo and embarkation",
          "[unit][lifetime][transfer][transport]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    Unit& transport = fixture.MakeUnit(giver, 4, 4, {"test_sea_chassis", "test_transport"});
    Unit& cargo = fixture.MakeUnit(giver, 4, 4, {"test_chassis"});
    cargo.EmbarkInto(transport);
    REQUIRE(cargo.IsEmbarked());
    REQUIRE(transport.GetCargo().size() == 1);

    Unit* pTransportAddress = &transport;
    Unit* pCargoAddress = &cargo;

    giver.TransferUnitTo(transport.GetUnitId(), receiver);

    // Both the carrier and its passenger moved to the receiver, still embarked together.
    CHECK(&pTransportAddress->GetFaction() == &receiver);
    CHECK(&pCargoAddress->GetFaction() == &receiver);
    CHECK(pCargoAddress->IsEmbarked());
    CHECK(pCargoAddress->GetCarrier() == pTransportAddress);
    REQUIRE(pTransportAddress->GetCargo().size() == 1);
    CHECK(pTransportAddress->GetCargo().front() == pCargoAddress);
}

TEST_CASE("Transferring an embarked passenger detaches it cleanly without destroying it",
          "[unit][lifetime][transfer][transport]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    Unit& transport = fixture.MakeUnit(giver, 4, 4, {"test_sea_chassis", "test_transport"});
    Unit& passenger = fixture.MakeUnit(giver, 4, 4, {"test_chassis"});
    passenger.EmbarkInto(transport);
    REQUIRE(passenger.IsEmbarked());

    Unit* pPassengerAddress = &passenger;

    // Subvert the passenger only — the carrier stays with the giver. Must not throw despite
    // the carrier still occupying the passenger's tile (no CanPlaceUnitOnTile re-check).
    CHECK_NOTHROW(giver.TransferUnitTo(passenger.GetUnitId(), receiver));

    CHECK_FALSE(pPassengerAddress->IsEmbarked());
    CHECK(&pPassengerAddress->GetFaction() == &receiver);
    CHECK(transport.GetCargo().empty());
    CHECK(&transport.GetFaction() == &giver);
}

TEST_CASE("Transferring a unit clears its home-base claim", "[unit][lifetime][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();
    BaseManager& home = fixture.MakeFactionBase(giver, 2, 2);

    Unit& unit = fixture.MakeUnit(giver, 3, 2, {"test_chassis"}, &home);
    REQUIRE(unit.GetHomeBase() == &home);

    giver.TransferUnitTo(unit.GetUnitId(), receiver);

    // A foreign home claim is invalid — cleared on transfer, not reassigned.
    CHECK(unit.GetHomeBase() == nullptr);
    CHECK(home.GetHomeUnits().GetUnits().empty());
}

// --- Base transfer / destroy ---------------------------------------------------------------

TEST_CASE("TransferBaseTo preserves BaseManager identity", "[base][lifetime][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(giver, 4, 4);
    BaseManager* pAddress = &base;
    const BaseId_t baseId = base.GetBaseId();

    REQUIRE(giver.GetBaseCount() == 1);
    REQUIRE(receiver.GetBaseCount() == 0);

    giver.TransferBaseTo(baseId, receiver);

    CHECK(giver.GetBaseCount() == 0);
    REQUIRE(receiver.GetBaseCount() == 1);
    BaseManager& rNowOwned = *receiver.Bases().begin();
    CHECK(&rNowOwned == pAddress);
    CHECK(rNowOwned.GetBaseId() == baseId);
    CHECK(&rNowOwned.GetFaction() == &receiver);
    CHECK(&pAddress->GetFaction() == &receiver);
}

TEST_CASE("BaseManager::OnDestroyed fires on raze but not on transfer",
          "[base][lifetime][signal]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    SECTION("transfer survives")
    {
        BaseManager& base = fixture.MakeFactionBase(giver, 4, 4);
        bool bDestroyed = false;
        base.OnDestroyed.Connect([&]() { bDestroyed = true; });

        giver.TransferBaseTo(base.GetBaseId(), receiver);

        CHECK_FALSE(bDestroyed);
    }

    SECTION("raze destroys")
    {
        BaseManager& base = fixture.MakeFactionBase(giver, 4, 4);
        const BaseId_t baseId = base.GetBaseId();
        bool bDestroyed = false;
        base.OnDestroyed.Connect([&]() { bDestroyed = true; });

        REQUIRE(giver.ExtractBase(baseId).has_value());

        CHECK(bDestroyed);
    }
}

// --- Deploy cooldown ledger ------------------------------------------------------------

TEST_CASE("Deploy cooldown migrates with a transferred base and does not leak to the giver",
          "[base][lifetime][deploy]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();

    BaseManager& coolingBase = fixture.MakeFactionBase(giver, 2, 2);
    coolingBase.GetBuildingManager().AddBuilding("test_facility_a");
    giver.DeployBuilding(coolingBase.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);

    BaseManager& readyBase = fixture.MakeFactionBase(giver, 6, 6);
    readyBase.GetBuildingManager().AddBuilding("test_facility_a");

    // Two copies faction-wide, one cooling: the giver already tracks readiness per copy.
    CHECK(giver.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 1);

    giver.TransferBaseTo(coolingBase.GetBaseId(), receiver);

    // Giver keeps its still-ready copy — the transferred base's old cooldown record must not
    // suppress it (no leak / no cross-base match).
    CHECK(giver.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 1);

    // Receiver inherits the cooldown under the new owner rather than the record vanishing.
    CHECK(receiver.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 0);
    // Once the cooldown has elapsed, the migrated record no longer suppresses the copy.
    CHECK(receiver.CountReadyBuildings("test_facility_a", /*missionYear*/ 100) == 1);
}

TEST_CASE("Extracting (razing) a base drops its deploy cooldown instead of leaking it",
          "[base][lifetime][deploy]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    BaseManager& doomedBase = fixture.MakeFactionBase(faction, 2, 2);
    doomedBase.GetBuildingManager().AddBuilding("test_facility_a");
    faction.DeployBuilding(doomedBase.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);

    BaseManager& survivingBase = fixture.MakeFactionBase(faction, 6, 6);
    survivingBase.GetBuildingManager().AddBuilding("test_facility_a");

    REQUIRE(faction.ExtractBase(doomedBase.GetBaseId()).has_value());

    // Only the surviving base's fresh copy remains; the razed base's cooldown record must be
    // gone, not lingering to suppress it.
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 1);
}

TEST_CASE("NotifyBuildingDestroyed only drops the record for its own base",
          "[base][lifetime][deploy]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    baseA.GetBuildingManager().AddBuilding("test_facility_a");
    faction.DeployBuilding(baseA.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);

    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);
    baseB.GetBuildingManager().AddBuilding("test_facility_a");
    faction.DeployBuilding(baseB.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);

    // Destroy baseA's copy specifically; baseB's still-cooling copy must be unaffected.
    baseA.GetBuildingManager().DestroyBuilding("test_facility_a");
    faction.NotifyBuildingDestroyed(baseA.GetBaseId(), "test_facility_a");

    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 0);
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 100) == 1);
}

TEST_CASE("NotifyBuildingDestroyed retires the still-cooling record, not an expired one",
          "[base][lifetime][deploy]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    // One base, two copies of the same building: an old deploy that has already come off
    // cooldown and a fresh one that has not. Records are only pruned once per turn (Upkeep),
    // so both are in the ledger when a copy is destroyed.
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("test_facility_a");
    base.GetBuildingManager().AddBuilding("test_facility_a");
    faction.DeployBuilding(base.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 10);
    faction.DeployBuilding(base.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);

    REQUIRE(faction.CountBuildings("test_facility_a") == 2);
    // At year 50 the first record has expired, so exactly one copy is ready.
    REQUIRE(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 50) == 1);

    base.GetBuildingManager().DestroyBuilding("test_facility_a");
    faction.NotifyBuildingDestroyed(base.GetBaseId(), "test_facility_a");

    // One copy left. Dropping the *expired* record instead would leave the year-100 cooldown
    // attached to a copy that no longer exists and report 0 ready — a copy suppressed by a
    // dead sibling's cooldown.
    REQUIRE(faction.CountBuildings("test_facility_a") == 1);
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 50) == 1);
}

TEST_CASE("CountReadyBuildings is a pure query: asking about a future year changes nothing",
          "[base][lifetime][deploy]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("test_facility_a");
    faction.DeployBuilding(base.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);

    // Ask about a year past the cooldown first (what a "ready in N years" UI preview does).
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 100) == 1);
    // The current-year answer must be unchanged by that question. When the query pruned as a
    // side effect, this returned 1 and the copy was silently off cooldown early.
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 0);
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 0);
}

TEST_CASE("PruneExpiredDeploys retires elapsed records without changing any answer",
          "[base][lifetime][deploy]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("test_facility_a");
    faction.DeployBuilding(base.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 10);

    REQUIRE(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 0);

    // Before expiry the record must survive the per-turn sweep.
    faction.PruneExpiredDeploys(/*missionYear*/ 5);
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 5) == 0);

    // After expiry it is retired — and the copy reads ready either way, so pruning is
    // observationally neutral (it only bounds ledger growth).
    faction.PruneExpiredDeploys(/*missionYear*/ 10);
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 10) == 1);
    CHECK(faction.CountReadyBuildings("test_facility_a", /*missionYear*/ 5) == 1);
}

// --- EventBridge auto-wiring -------------------------------------------------------------

TEST_CASE("EventBridge wires bases via OnBaseAdded, without a founding-only call site",
          "[base][lifetime][eventbridge]")
{
    FactionFixture fixture;
    EventBus bus;
    EventBridge bridge(bus);

    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();
    // Mirrors Engine: connect once per faction at composition, not per call site.
    giver.OnBaseAdded.Connect([&](BaseManager& rBase) { bridge.WireBase(rBase); });
    receiver.OnBaseAdded.Connect([&](BaseManager& rBase) { bridge.WireBase(rBase); });

    int gainedEvents = 0;
    bus.Subscribe<EvBaseGainedPop>([&](const EvBaseGainedPop&) { ++gainedEvents; });

    // Founded (not the Engine-only path) — still wired because AddBase emits OnBaseAdded.
    BaseManager& base = fixture.MakeFactionBase(giver, 4, 4);
    base.GetPopulation().AddPop();
    CHECK(gainedEvents == 1);

    // Transfer to a faction that also connected OnBaseAdded → WireBase. Idempotency by
    // baseId must prevent a second connection from doubling published events.
    giver.TransferBaseTo(base.GetBaseId(), receiver);
    base.GetPopulation().AddPop();
    CHECK(gainedEvents == 2);
}

// --- WorkerAssignmentManager teardown ------------------------------------------------------
// (HomeBaseIndex transfer-survival coverage lives in HomeBaseTests.cpp, beside the rest of
// that index's lifecycle tests.)

TEST_CASE("Extracting a base with assigned workers releases their tile claims cleanly",
          "[base][lifetime][worker]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // The base's initial pops are auto-assigned to workable tiles at construction.
    Pop& rWorker = FirstWorkerPop_(base);
    const Tile* pWorkedTile = rWorker.GetTile();
    REQUIRE(pWorkedTile != nullptr);
    REQUIRE(fixture.map.GetWorkedTiles().IsWorked(*pWorkedTile));

    REQUIRE(faction.ExtractBase(base.GetBaseId()).has_value());

    // No crash tearing the manager down with live claims (~WorkerAssignmentManager clears
    // them before ~PopulationManager destroys the pops), and the tile is free again.
    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(*pWorkedTile));

    // A second base overlapping the freed tile can claim it — displacement/auto-assign still
    // works normally with no stale handler left pointing at the destroyed manager.
    Faction& otherFaction = fixture.MakeFaction();
    BaseManager& other = fixture.MakeFactionBase(otherFaction, pWorkedTile->GetX(),
                                                 pWorkedTile->GetY());
    (void)other;
    CHECK(fixture.map.GetWorkedTiles().IsWorked(*pWorkedTile));
}
