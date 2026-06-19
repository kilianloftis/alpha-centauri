#pragma once

namespace ac
{

struct PopTypeConfig;

// Forward declaration - defined in game/faction/Base.h
struct TileResources_t;

// Specialist generation output (not tile-based)
struct SpecialistOutput_t
{
    int econ;   // Economic output
    int labs;   // Research output
    int psych;  // Psych output
};

// A single population unit. Behaviour is entirely driven by its PopTypeConfig.
class Pop
{
public:
    explicit Pop(const PopTypeConfig& rConfig, int id = -1);
    ~Pop();

    // Type id string matching the config (e.g. "Worker", "Librarian")
    const char* GetPopType() const;

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

    // Swap this pop's type config in-place, preserving id.
    // Clears tileId if the new type is not a worker.
    void Convert(const PopTypeConfig& rConfig);

    // Stable identity — assigned by PopContainer at creation, survives ConvertTo
    int GetId() const;
    void SetId(int id);

    // Tile assignment (only meaningful when IsWorker() is true)
    void SetTileId(int tileId);
    int GetTileId() const;

    // Apply this pop's tile multipliers to raw tile resources.
    // Returns modified resources based on pop type's tile_multipliers config.
    // Only meaningful when IsWorker() is true.
    TileResources_t ApplyTileMultipliers(const TileResources_t& resources) const;

    // Get specialist generation output (econ/labs/psych).
    // This is direct output from specialists, not from tiles.
    SpecialistOutput_t GetSpecialistOutput() const;

private:
    const PopTypeConfig* m_pConfig;
    int m_id;
    int m_tileId;
};

} // namespace ac
