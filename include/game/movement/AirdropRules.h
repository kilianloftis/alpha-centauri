#pragma once

namespace ac
{

class Tile;
class Unit;
class WorldMap;
class TileEffectsContext;
class Faction;

enum class AirdropFailReason_t
{
    None,
    NotCapable,
    NotOnLaunchPad,
    NoMovesRemaining,
    AlreadyAirdropped,
    OutOfRange,
    CannotEnter,
    EnemyOccupied,
    Interdicted,
};

struct AirdropEligibility_t
{
    AirdropFailReason_t failReason = AirdropFailReason_t::None;
    bool Ok() const { return failReason == AirdropFailReason_t::None; }
};

// True when rTile provides airdrop_launch for rFactionId (Base / Airbase, not carrier decks).
bool TileIsAirdropLaunchPad(const Tile& rTile, const WorldMap& rWorldMap, int factionId);

// True when rOccupant makes the tile illegal for rDropper to airdrop onto (same hostile
// rules as StepEvaluator: other faction; embarked cargo only blocks on a Base tile).
bool OccupantBlocksAirdrop(const Unit& rDropper, const Unit& rOccupant);

struct AirdropLanding_t
{
    int hpAfter = 0;
    bool bEndsTurn = false;
};

// Away from the faction's own launch pads the drop costs AirdropLandingDamage percent of max HP,
// never the last point. A non-combat unit spends its turn landing.
AirdropLanding_t AirdropLanding(const Unit& rUnit, const Tile& rDest, const WorldMap& rWorldMap);

// True when CollectAreaEffects(rDest) includes a hostile-owned airdrop_interdiction RuleFlag
// (ThisTile aura, typically with radius — stock Air Superiority).
bool IsAirdropInterdicted(const Unit& rDropper, const Tile& rDest,
                          const TileEffectsContext& rTileEffects);

// Unit-side gates only: airdrop flag, currently on launch pad, full moves remaining, and
// has not already airdropped this turn.
AirdropEligibility_t CanAttemptAirdrop(const Unit& rUnit);

// Full destination legality for a drop from rUnit's current tile onto rDest (no adjacency /
// ZOC). Does not mutate.
AirdropEligibility_t CanAirdropTo(const Unit& rUnit, const Tile& rDest, const WorldMap& rWorldMap,
                                  const TileEffectsContext& rTileEffects);

} // namespace ac
