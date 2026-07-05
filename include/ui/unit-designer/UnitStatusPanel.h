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
        const UnitManager* pUnitManager,
        WindowLayout_t layout
    );
    ~UnitStatusPanel() override = default;

    void Render(Graphics& rGraphics) override;

private:
    std::function<const UnitDesign*()> m_getSelectedDesign;
    const UnitManager* m_pUnitManager;
};

} // namespace ac
