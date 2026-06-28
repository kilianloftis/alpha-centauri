#pragma once

#include "ui/IGameView.h"
#include "ui/unit-designer/UnitDesignerState.h"
#include "game/units/UnitComponentConfig.h"
#include <functional>

namespace ac
{

class Military;
class UnitComponentRegistry;
class UnitManager;
class UnitDesign;

class UnitDesignerView : public IGameView
{
public:
    UnitDesignerView(
        Military& rMilitary,
        const UnitComponentRegistry& rRegistry,
        const UnitManager* pUnitManager,
        WindowLayout_t layout
    );
    ~UnitDesignerView() override = default;

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void ShowComponentSelector_(UnitComponentType_t type, std::function<void(const UnitComponentConfig_t&)> onSelected);
    void HandleSaveDesign_();

    Military& m_rMilitary;
    const UnitComponentRegistry& m_rRegistry;
    const UnitDesign* m_pSelectedDesign = nullptr;
    UnitDesignerState_t m_state;

    // Overall layout regions (ratios of full view)
    static constexpr RatioLayout_t k_StatusPanelRatio      {0.0f,  0.0f,  0.15f, 1.0f};
    static constexpr RatioLayout_t k_TopDesignerPanelRatio {0.15f, 0.0f,  0.85f, 0.7f};
    static constexpr RatioLayout_t k_BottomDesignListRatio {0.15f, 0.7f,  0.85f, 0.3f};

    // Top designer panel column splits
    static constexpr RatioLayout_t k_LeftColumnRatio  {0.0f,  0.0f, 0.25f, 1.0f};
    static constexpr RatioLayout_t k_CenterColumnRatio{0.25f, 0.0f, 0.5f,  1.0f};
    static constexpr RatioLayout_t k_RightColumnRatio {0.75f, 0.0f, 0.25f, 1.0f};

    // Three vertical slot rows within each side column
    static constexpr RatioLayout_t k_SlotRow0Ratio{0.0f, 0.0f,       1.0f, 0.3334f};
    static constexpr RatioLayout_t k_SlotRow1Ratio{0.0f, 0.3334f,    1.0f, 0.3334f};
    static constexpr RatioLayout_t k_SlotRow2Ratio{0.0f, 0.6667f,    1.0f, 0.3333f};

    // Component selector popup
    static constexpr RatioLayout_t k_SelectorPopupRatio{0.2f, 0.1f, 0.6f, 0.8f};
};

} // namespace ac
