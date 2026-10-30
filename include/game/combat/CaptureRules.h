#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;
class Tile;
class Unit;

enum class CapturedMoves_t
{
    Spent,
    Refilled,
};

// One unit changing hands, and how it arrives. Cargo travels with its carrier.
struct CapturedUnit_t
{
    Unit* pUnit = nullptr;
    // A base of the captor, or none.
    std::optional<BaseId_t> homeBaseId;
    // Moved here, onto pCarrier when set; nullptr keeps its tile.
    const Tile* pDestination = nullptr;
    Unit* pCarrier = nullptr;
    bool bRestoresHp = false;
    CapturedMoves_t moves = CapturedMoves_t::Spent;
};

// HP rDefender must lose before Marine Detachment boarding rolls start: board_hp_limit of its HP
// now. Unset when board_chance is 0 here or the limit rounds to 0 (terranx-map.md "Marine
// Detachment").
std::optional<int> BoardingThreshold(const Unit& rAttacker, const Unit& rDefender);

// rAttacker's board_chance against rDefender.
int BoardingChance(const Unit& rAttacker, const Unit& rDefender);

// Every unit on rTile goes to rAttacker's faction, keeping its damage, with spent moves. Home base
// is the nearest base of any owner when the captor owns it and it holds at least
// boarding_home_base_min_pop (combat.json); otherwise none.
std::vector<CapturedUnit_t> BoardingCapture(const Unit& rAttacker, const Tile& rTile);

// A combat kill by rAttacker that takes rDefeated instead: captured_on_defeat, an attacker that is
// neither wild native life nor SingleUse, standing on land or beside a carrier with room. The unit
// joins rAttacker's tile at full HP with no home base and spent moves. Unset destroys it.
std::optional<CapturedUnit_t> DefeatedUnitCapture(const Unit& rAttacker, Unit& rDefeated);

// A unit rActor takes by Subvert Unit: home base is rActor's nearest base, and it keeps its
// damage with full moves (terranx-map.md "Probe Mind Control on a unit").
CapturedUnit_t SubvertCapture(const Faction& rActor, Unit& rTarget);

// Friction the victim gains against the captor for taking its artifacts:
// artifact_capture_friction for an AI victim outside Vendetta, else 0.
int ArtifactCaptureFriction(FactionId_t captor, FactionId_t victim);

} // namespace ac
