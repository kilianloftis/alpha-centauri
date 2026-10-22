#pragma once

#include "game/base/BaseTypes.h"

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

// Derived values the panels show, computed once per key change since each is costly. Tile
// state is not in the key: the turn cannot advance while this overlay is open.
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
