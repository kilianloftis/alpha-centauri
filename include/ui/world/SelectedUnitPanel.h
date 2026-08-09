#pragma once

#include "ui/UIElement.h"

namespace ac
{

class Graphics;
class Unit;

class SelectedUnitPanel : public UIElement
{
public:
    explicit SelectedUnitPanel(WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

    void SetSelectedUnit(const Unit* pUnit) { m_pSelectedUnit = pUnit; }
    const Unit* GetSelectedUnit() const { return m_pSelectedUnit; }

private:
    void DrawBackground_(Graphics& rGraphics) const;
    void DrawEmptyState_(Graphics& rGraphics) const;
    void DrawUnitIcon_(Graphics& rGraphics, float iconX, float iconY, float iconSize) const;
    float DrawName_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    float DrawStats_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    float DrawMoves_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    // Advances textY unchanged when the design does not track fuel.
    float DrawFuel_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    float DrawOrders_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    void DrawHomeBase_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;

    const Unit* m_pSelectedUnit = nullptr;
};

} // namespace ac
