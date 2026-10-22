#pragma once

#include "game/combat/CombatResolver.h"
#include "graphics/Graphics.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"

#include <chrono>
#include <cstddef>
#include <optional>

namespace ac
{

class WorldDisplay;

// Plays back a resolved fight round by round; Resolve has already applied everything.
class CombatPresentation
{
public:
    explicit CombatPresentation(
        int damageFlashMs = Style().combatPresentation.defaultDamageFlashMs,
        int interRoundDelayMs = Style().combatPresentation.defaultInterRoundDelayMs);

    // Copies rResult and the combatants' tiles. Empty rounds clear it unless bBombardPlayback,
    // which still flashes the defender's tile.
    void Begin(const CombatResult_t& rResult,
               const Tile& rAttackerTile,
               const Tile& rDefenderTile);
    void Clear();

    bool IsActive() const { return m_phase != Phase_t::Idle; }

    // Advance flash / inter-round timers. Call once per frame while active.
    void Update();

    // Unit taking damage in the current flash beat, if any.
    std::optional<UnitId_t> GetFlashingUnitId() const;

    const CombatResult_t& GetResult() const { return m_result; }

    // Round currently being shown (flash or post-flash delay), if playback is active.
    std::optional<size_t> GetDisplayedRoundIndex() const;
    const CombatRound_t* GetDisplayedRound() const;

    // The hit graphic over the damaged unit, at its last drawn position or its pre-fight tile.
    void Render(Graphics& rGraphics,
                const WorldDisplay& rDisplay) const;

private:
    enum class Phase_t
    {
        Idle,
        Flashing,
        InterRoundDelay,
    };

    void StartRound_(size_t roundIndex);
    void AdvanceAfterFlash_();
    void DrawBombardOverlay_(Graphics& rGraphics, const WorldDisplay& rDisplay) const;
    UnitId_t DamagedUnitId_(const CombatRound_t& rRound) const;
    const Tile* TileForUnitId_(UnitId_t unitId) const;
    bool WasDestroyed_(UnitId_t unitId) const;

    int m_damageFlashMs;
    int m_interRoundDelayMs;

    CombatResult_t m_result;
    const Tile* m_pAttackerTile = nullptr;
    const Tile* m_pDefenderTile = nullptr;
    bool m_bBombardPlayback = false;

    Phase_t m_phase = Phase_t::Idle;
    size_t m_roundIndex = 0;
    std::chrono::steady_clock::time_point m_phaseStart;
};

} // namespace ac
