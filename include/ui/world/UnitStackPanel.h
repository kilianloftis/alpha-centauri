#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <vector>

namespace ac
{

class Graphics;
class Unit;

// Horizontal list of units on the selected map tile. Each entry shows a unit icon with
// FormatCombatRating (a-d-m) underneath. Clicking an entry selects that unit.
class UnitStackPanel : public UIElement
{
public:
    using UnitClickCallback_t = std::function<void(Unit&)>;

    UnitStackPanel(WindowLayout_t layout, UnitClickCallback_t onUnitClicked);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    // The visible window follows pSelectedUnit, so a stack wider than the panel stays fully
    // reachable through the select-next-unit cycle rather than being silently truncated.
    void SetUnits(std::vector<Unit*> units, const Unit* pSelectedUnit);

private:
    struct Slot_t
    {
        Rectangle_t rect;
        Unit* pUnit = nullptr;
    };

    bool HasHiddenUnits() const;
    void ScrollSelectedIntoView_();
    void DrawBackground_(Graphics& rGraphics) const;
    void CacheSlots_();
    void DrawSlot_(Graphics& rGraphics, const Slot_t& rSlot) const;

    UnitClickCallback_t m_onUnitClicked;
    std::vector<Unit*> m_units;
    const Unit* m_pSelectedUnit = nullptr;
    std::vector<Slot_t> m_slots;
    // Index of the leftmost unit currently laid out.
    size_t m_scrollOffset = 0;
};

} // namespace ac
