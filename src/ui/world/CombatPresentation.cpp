#include "ui/world/CombatPresentation.h"

#include "game/map/Tile.h"
#include "ui/world/MapViewport.h"
#include "ui/world/UnitMarkerRenderer.h"
#include "ui/world/WorldDisplay.h"

namespace ac
{

CombatPresentation::CombatPresentation(int damageFlashMs, int interRoundDelayMs)
    : m_damageFlashMs(damageFlashMs)
    , m_interRoundDelayMs(interRoundDelayMs)
{
}

void CombatPresentation::Begin(const CombatResult_t& rResult,
                               const Tile& rAttackerTile,
                               const Tile& rDefenderTile)
{
    Clear();
    if (rResult.rounds.empty() && !rResult.bBombardPlayback)
    {
        return;
    }

    m_result = rResult;
    m_bBombardPlayback = rResult.bBombardPlayback;
    m_pAttackerTile = &rAttackerTile;
    m_pDefenderTile = &rDefenderTile;
    if (rResult.rounds.empty())
    {
        m_roundIndex = 0;
        m_phase = Phase_t::Flashing;
        m_phaseStart = std::chrono::steady_clock::now();
        return;
    }
    StartRound_(0);
}

void CombatPresentation::Clear()
{
    m_result = {};
    m_pAttackerTile = nullptr;
    m_pDefenderTile = nullptr;
    m_bBombardPlayback = false;
    m_phase = Phase_t::Idle;
    m_roundIndex = 0;
}

void CombatPresentation::Update()
{
    if (m_phase == Phase_t::Idle)
    {
        return;
    }

    const auto elapsed = std::chrono::steady_clock::now() - m_phaseStart;
    if (m_phase == Phase_t::Flashing)
    {
        if (elapsed >= std::chrono::milliseconds(m_damageFlashMs))
        {
            AdvanceAfterFlash_();
        }
        return;
    }

    // InterRoundDelay
    if (elapsed >= std::chrono::milliseconds(m_interRoundDelayMs))
    {
        StartRound_(m_roundIndex + 1);
    }
}

std::optional<UnitId_t> CombatPresentation::GetFlashingUnitId() const
{
    if (m_bBombardPlayback || m_phase != Phase_t::Flashing
        || m_roundIndex >= m_result.rounds.size())
    {
        return std::nullopt;
    }
    return DamagedUnitId_(m_result.rounds[m_roundIndex]);
}

std::optional<size_t> CombatPresentation::GetDisplayedRoundIndex() const
{
    if (m_phase == Phase_t::Idle || m_roundIndex >= m_result.rounds.size())
    {
        return std::nullopt;
    }
    return m_roundIndex;
}

const CombatRound_t* CombatPresentation::GetDisplayedRound() const
{
    const std::optional<size_t> index = GetDisplayedRoundIndex();
    if (!index)
    {
        return nullptr;
    }
    return &m_result.rounds[*index];
}

void CombatPresentation::Render(Graphics& rGraphics,
                                const WorldDisplay& rDisplay) const
{
    if (m_bBombardPlayback && m_phase == Phase_t::Flashing)
    {
        DrawBombardOverlay_(rGraphics, rDisplay);
        return;
    }

    const std::optional<UnitId_t> flashingId = GetFlashingUnitId();
    if (!flashingId)
    {
        return;
    }

    if (const std::optional<Rectangle_t> cached = rDisplay.MarkerRectOf(*flashingId))
    {
        UnitMarkerRenderer::DrawHitOverlay(rGraphics, *cached);
        return;
    }

    // Unit was destroyed during Resolve — never drawn this frame, so place a ghost hit on
    // its pre-fight tile using the same marker layout.
    if (!WasDestroyed_(*flashingId))
    {
        return;
    }

    const Tile* pTile = TileForUnitId_(*flashingId);
    if (!pTile)
    {
        return;
    }

    const auto origin = rDisplay.GetViewport().PixelOriginOf(pTile->GetX(), pTile->GetY());
    if (!origin)
    {
        return;
    }

    UnitMarkerRenderer::DrawHitOverlay(
        rGraphics,
        UnitMarkerRenderer::MarkerRectOnTile(origin->first, origin->second,
                                             rDisplay.GetEffectiveTileSize(), 0));
}

void CombatPresentation::DrawBombardOverlay_(Graphics& rGraphics,
                                               const WorldDisplay& rDisplay) const
{
    if (!m_pDefenderTile)
    {
        return;
    }
    const auto origin = rDisplay.GetViewport().PixelOriginOf(
        m_pDefenderTile->GetX(), m_pDefenderTile->GetY());
    if (!origin)
    {
        return;
    }
    const Rectangle_t rect = UnitMarkerRenderer::MarkerRectOnTile(
        origin->first, origin->second, rDisplay.GetEffectiveTileSize(), 0);
    const auto& s = Style().bombardPresentation;
    rGraphics.DrawFilledRect(rect.x, rect.y, rect.width, rect.height, s.overlayFill);
    rGraphics.DrawRect(rect.x, rect.y, rect.width, rect.height, s.overlayBorder,
                       s.overlayBorderWidth);
}

void CombatPresentation::StartRound_(size_t roundIndex)
{
    if (roundIndex >= m_result.rounds.size())
    {
        Clear();
        return;
    }

    m_roundIndex = roundIndex;
    m_phase = Phase_t::Flashing;
    m_phaseStart = std::chrono::steady_clock::now();
}

void CombatPresentation::AdvanceAfterFlash_()
{
    if (m_roundIndex + 1 >= m_result.rounds.size())
    {
        Clear();
        return;
    }

    m_phase = Phase_t::InterRoundDelay;
    m_phaseStart = std::chrono::steady_clock::now();
}

UnitId_t CombatPresentation::DamagedUnitId_(const CombatRound_t& rRound) const
{
    // Round winner deals damage; the other side takes the hit overlay.
    return rRound.roundWinner == CombatSide_t::Attacker
               ? m_result.defenderId
               : m_result.attackerId;
}

const Tile* CombatPresentation::TileForUnitId_(UnitId_t unitId) const
{
    if (unitId == m_result.attackerId)
    {
        return m_pAttackerTile;
    }
    if (unitId == m_result.defenderId)
    {
        return m_pDefenderTile;
    }
    return nullptr;
}

bool CombatPresentation::WasDestroyed_(UnitId_t unitId) const
{
    return (unitId == m_result.attackerId && m_result.bAttackerDestroyed)
        || (unitId == m_result.defenderId && m_result.bDefenderDestroyed);
}

} // namespace ac
