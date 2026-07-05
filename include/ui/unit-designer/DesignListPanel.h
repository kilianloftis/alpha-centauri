#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class Military;
class UnitDesign;

class DesignListPanel : public UIElement
{
public:
    DesignListPanel(
        const Military* pMilitary,
        WindowLayout_t layout,
        std::function<void(const UnitDesign*)> onDesignSelected = nullptr
    );
    ~DesignListPanel() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    void SetSelectedDesign(const UnitDesign* pDesign) { m_pSelectedDesign = pDesign; }

private:
    const Military* m_pMilitary;
    std::function<void(const UnitDesign*)> m_onDesignSelected;
    const UnitDesign* m_pSelectedDesign = nullptr;
};

} // namespace ac
