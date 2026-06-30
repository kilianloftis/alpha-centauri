#pragma once

#include "game/faction/base/BaseTypes.h"

namespace ac
{

class Tile;
struct PopTypeConfig_t;

// Specialist generation output (not tile-based)
struct SpecialistOutput_t
{
    int econ;   // Economic output
    int labs;   // Research output
    int psych;  // Psych output
};

// A single population unit. Behaviour is entirely driven by its PopTypeConfig_t.
class Pop
{
public:
    explicit Pop(const PopTypeConfig_t& rConfig);
    ~Pop();

    // Type id string matching the config (e.g. "Worker", "Librarian")
    const char* GetPopType() const;

    // The full config backing this pop's current type.
    const PopTypeConfig_t& GetConfig() const;

    // True if this pop can work a tile (can_work_tile in config)
    bool IsWorker() const;

    // True if this pop contributes to riot (riot_contribution > 0)
    bool IsDrone() const;

    // True if this pop is a specialist (!can_work_tile && !IsDrone())
    bool IsSpecialist() const;

    // True if the player can manually assign this pop type
    bool IsPlayerAssignable() const;

    // Riot and golden age contribution values from config
    int GetRiotContribution() const;
    int GetGoldenAgeContribution() const;

    // Swap this pop's type config in-place.
    // Clears tile assignment if the new type is not a worker.
    void Convert(const PopTypeConfig_t& rConfig);

    // Tile assignment (only meaningful when IsWorker() is true)
    // nullptr means unassigned.
    void SetTile(const Tile* pTile);
    const Tile* GetTile() const;

    // User assignment flag (e.g., manually assigned by the player).
    // Cleared automatically when the tile is unassigned.
    void SetUserAssigned(bool bUserAssigned);
    bool IsUserAssigned() const;

    // Apply this pop's tile multipliers to raw tile resources.
    // Returns modified resources based on pop type's tile_multipliers config.
    // Only meaningful when IsWorker() is true.
    TileResources_t ApplyTileMultipliers(const TileResources_t& resources) const;

    // Get specialist generation output (econ/labs/psych).
    // This is direct output from specialists, not from tiles.
    SpecialistOutput_t GetSpecialistOutput() const;

private:
    const PopTypeConfig_t* m_pConfig;
    const Tile* m_pTile;
    bool m_bUserAssigned;
};

} // namespace ac
