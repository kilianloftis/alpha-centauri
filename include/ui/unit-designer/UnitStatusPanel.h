#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class UnitDesign;
class UnitManager;

class UnitStatusPanel : public UIElement
{
public:
    UnitStatusPanel(
        std::function<const UnitDesign*()> getSelectedDesign,
        const UnitManager& rUnitManager,
        WindowLayout_t layout
    );
    ~UnitStatusPanel() override = default;

    void Render(Graphics& rGraphics) override;

private:
    std::function<const UnitDesign*()> m_getSelectedDesign;
    int CountActive_(const UnitDesign& rDesign) const;

    const UnitManager& m_rUnitManager;
};

} // namespace ac
