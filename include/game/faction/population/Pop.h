#pragma once

namespace ac
{

struct PopTypeConfig;

// Production output from a pop
struct PopProduction_t
{
    int nutrients;
    int energy;
    int minerals;
    int econ;   // Economic output (mainly from specialists)
    int labs;   // Research output (mainly from specialists)
    int psych;  // Psych output (mainly from talents/specialists)
};

// A single population unit. Behaviour is entirely driven by its PopTypeConfig.
class Pop
{
public:
    explicit Pop(const PopTypeConfig& rConfig);
    ~Pop();

    // Type id string matching the config (e.g. "Worker", "Librarian")
    const char* GetPopType() const;

    // True if this pop can work a tile (can_work_tile in config)
    bool IsWorker() const;

    // True if this pop contributes to riot (riot_contribution > 0)
    bool IsDrone() const;

    // True if this pop is a specialist (!can_work_tile && !IsDrone())
    bool IsSpecialist() const;

    // Riot and golden age contribution values from config
    int GetRiotContribution() const;
    int GetGoldenAgeContribution() const;

    // Tile assignment (only meaningful when IsWorker() is true)
    void SetTileId(int tileId);
    int GetTileId() const;

    // Compute production for this pop.
    // tileResources is multiplied by tile_multipliers then added to generation values.
    PopProduction_t GetProduction(const PopProduction_t& tileResources) const;

private:
    const PopTypeConfig* m_pConfig;
    int m_tileId;
};

} // namespace ac
