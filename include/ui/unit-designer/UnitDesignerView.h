#pragma once

#include "ui/IGameView.h"
#include "ui/unit-designer/UnitDesignerState.h"

namespace ac
{

struct UnitComponentConfig_t;
class Military;
class UnitComponentRegistry;
class UnitSlotRegistry;
class UnitManager;
class UnitDesign;

class UnitDesignerView : public IGameView
{
public:
    UnitDesignerView(
        Military& rMilitary,
        const UnitComponentRegistry& rComponentRegistry,
        const UnitSlotRegistry& rSlotRegistry,
        const UnitManager* pUnitManager,
        WindowLayout_t layout
    );
    ~UnitDesignerView() override = default;

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void ShowComponentSelector_(
        const std::string& rComponentType,
        std::function<void(const UnitComponentConfig_t&)> onSelected
    );
    void HandleSaveDesign_();

    Military& m_rMilitary;
    const UnitComponentRegistry& m_rComponentRegistry;
    const UnitSlotRegistry& m_rSlotRegistry;
    const UnitDesign* m_pSelectedDesign = nullptr;
    UnitDesignerState_t m_state;

    static constexpr RatioLayout_t k_StatusPanelRatio      {0.0f,  0.0f,  0.15f, 1.0f};
    static constexpr RatioLayout_t k_TopDesignerPanelRatio {0.15f, 0.0f,  0.85f, 0.7f};
    static constexpr RatioLayout_t k_BottomDesignListRatio {0.15f, 0.7f,  0.85f, 0.3f};

    static constexpr RatioLayout_t k_LeftColumnRatio  {0.0f,  0.0f, 0.25f, 1.0f};
    static constexpr RatioLayout_t k_CenterColumnRatio{0.25f, 0.0f, 0.5f,  1.0f};
    static constexpr RatioLayout_t k_RightColumnRatio {0.75f, 0.0f, 0.25f, 1.0f};

    static constexpr RatioLayout_t k_SelectorPopupRatio{0.2f, 0.1f, 0.6f, 0.8f};
};

} // namespace ac
