#pragma once

#include "ui/UIElement.h"
#include <functional>
#include <vector>

namespace ac
{

class Military;
class UnitDesign;

class DesignListPanel : public UIElement
{
public:
    DesignListPanel(
        const Military& rMilitary,
        WindowLayout_t layout,
        std::function<void(const UnitDesign*)> onDesignSelected
    );
    ~DesignListPanel() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    void SetSelectedDesign(const UnitDesign* pDesign) { m_pSelectedDesign = pDesign; }

private:
    struct DesignBox_t
    {
        const UnitDesign* pDesign;
        Rectangle_t rect;
    };

    // The boxes that fit in the panel, left to right.
    std::vector<DesignBox_t> LayoutBoxes_() const;

    const Military& m_rMilitary;
    std::function<void(const UnitDesign*)> m_onDesignSelected;
    const UnitDesign* m_pSelectedDesign = nullptr;
};

} // namespace ac
