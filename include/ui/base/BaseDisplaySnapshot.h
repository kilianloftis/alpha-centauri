#pragma once

#include "game/faction/base/BaseTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ac
{

class BaseManager;
class Tile;

// Who, if anyone, is working a tile in a base's radius. Overlapping radii are normal, so
// "not worked by me" and "free to take" are different questions.
enum class TileWorkState_t
{
    Free,
    WorkedByThisBase,
    WorkedByOther,
};

struct TileDisplay_t
{
    TileWorkState_t workState = TileWorkState_t::Free;
    TileYieldView_t yield;
};

// What the snapshot was built from. Every input that can move a displayed value while a
// BaseView is open, and nothing else — see BaseDisplaySnapshot_t.
struct BaseDisplayKey_t
{
    uint64_t effectsVersion = 0;
    uint64_t workedTileRevision = 0;
    uint64_t populationRevision = 0;
    uint64_t homeUnitRevision = 0;
    const void* pCurrentProduction = nullptr;

    bool operator==(const BaseDisplayKey_t&) const = default;
};

// The derived base values the panels display. Each one costs a full ResourceManager worked-tile
// pass or a per-tile yield resolution, so they are computed once per key change rather than per
// panel per frame. Cheap reads (stockpiles, base name, population size) stay live.
//
// Tile state (terrain, improvements, resources) is absent from the key because it cannot change
// while this view is open: terraforming resolves on turn advance, and UIManager refuses to
// advance the turn while an overlay covers the map — pinned by "The turn gate closes for an
// overlay and for an in-view modal" in tests/ui/UIManagerTests.cpp.
struct BaseDisplaySnapshot_t
{
    BaseDisplayKey_t key;

    int nutrientProduction = 0;
    int nutrientsRequired = 0;
    // Minerals that will bank into the queued item this turn (after unit support).
    int mineralProduction = 0;
    int mineralCost = 0;
    bool bHasProduction = false;
    std::string productionName;

    std::unordered_map<const Tile*, TileDisplay_t> tiles;
};

BaseDisplayKey_t ReadBaseDisplayKey(const BaseManager& rBase);

BaseDisplaySnapshot_t BuildBaseDisplaySnapshot(const BaseManager& rBase);

} // namespace ac
