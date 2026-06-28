#include "ui/unit-designer/UnitDesignerView.h"
#include "ui/unit-designer/ComponentSlotDisplay.h"
#include "ui/unit-designer/ComponentSelectorPopup.h"
#include "ui/unit-designer/DesignStatsDisplay.h"
#include "ui/unit-designer/DesignListPanel.h"
#include "ui/unit-designer/UnitStatusPanel.h"
#include "game/faction/Military.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "input/Input.h"

namespace ac
{

UnitDesignerView::UnitDesignerView(
    Military& rMilitary,
    const UnitComponentRegistry& rRegistry,
    const UnitManager* pUnitManager,
    WindowLayout_t layout
)
    : IGameView(layout)
    , m_rMilitary(rMilitary)
    , m_rRegistry(rRegistry)
{
    const WindowLayout_t statusLayout  = ResolveLayout(m_layout, k_StatusPanelRatio);
    const WindowLayout_t topLayout     = ResolveLayout(m_layout, k_TopDesignerPanelRatio);
    const WindowLayout_t bottomLayout  = ResolveLayout(m_layout, k_BottomDesignListRatio);

    const WindowLayout_t leftColLayout   = ResolveLayout(topLayout, k_LeftColumnRatio);
    const WindowLayout_t centerColLayout = ResolveLayout(topLayout, k_CenterColumnRatio);
    const WindowLayout_t rightColLayout  = ResolveLayout(topLayout, k_RightColumnRatio);

    // Left column: Chassis, Weapon, Armour (top to bottom)
    m_elements.push_back(std::make_unique<ComponentSlotDisplay>(
        "Chassis",
        [this]() { return m_state.pChassis; },
        ResolveLayout(leftColLayout, k_SlotRow0Ratio),
        [this]() { ShowComponentSelector_(UnitComponentType_t::Chassis, [this](const UnitComponentConfig_t& rComp) { m_state.pChassis = &rComp; }); }
    ));
    m_elements.push_back(std::make_unique<ComponentSlotDisplay>(
        "Weapon",
        [this]() { return m_state.pWeapon; },
        ResolveLayout(leftColLayout, k_SlotRow1Ratio),
        [this]() { ShowComponentSelector_(UnitComponentType_t::Weapon, [this](const UnitComponentConfig_t& rComp) { m_state.pWeapon = &rComp; }); }
    ));
    m_elements.push_back(std::make_unique<ComponentSlotDisplay>(
        "Armour",
        [this]() { return m_state.pArmour; },
        ResolveLayout(leftColLayout, k_SlotRow2Ratio),
        [this]() { ShowComponentSelector_(UnitComponentType_t::Armour, [this](const UnitComponentConfig_t& rComp) { m_state.pArmour = &rComp; }); }
    ));

    // Center column: design stats and save button
    m_elements.push_back(std::make_unique<DesignStatsDisplay>(
        &m_state,
        centerColLayout,
        [this]() { HandleSaveDesign_(); }
    ));

    // Right column: Reactor, Ability 1, Ability 2 (top to bottom)
    m_elements.push_back(std::make_unique<ComponentSlotDisplay>(
        "Reactor",
        [this]() { return m_state.pReactor; },
        ResolveLayout(rightColLayout, k_SlotRow0Ratio),
        [this]() { ShowComponentSelector_(UnitComponentType_t::Reactor, [this](const UnitComponentConfig_t& rComp) { m_state.pReactor = &rComp; }); }
    ));
    m_elements.push_back(std::make_unique<ComponentSlotDisplay>(
        "Ability 1",
        [this]() { return m_state.pAbility1; },
        ResolveLayout(rightColLayout, k_SlotRow1Ratio),
        [this]() { ShowComponentSelector_(UnitComponentType_t::Ability, [this](const UnitComponentConfig_t& rComp) { m_state.pAbility1 = &rComp; }); }
    ));
    m_elements.push_back(std::make_unique<ComponentSlotDisplay>(
        "Ability 2",
        [this]() { return m_state.pAbility2; },
        ResolveLayout(rightColLayout, k_SlotRow2Ratio),
        [this]() { ShowComponentSelector_(UnitComponentType_t::Ability, [this](const UnitComponentConfig_t& rComp) { m_state.pAbility2 = &rComp; }); }
    ));

    // Bottom: list of all saved designs
    m_elements.push_back(std::make_unique<DesignListPanel>(
        &m_rMilitary,
        bottomLayout,
        [this](const UnitDesign* pDesign)
        {
            m_pSelectedDesign = pDesign;
            if (pDesign)
            {
                m_state.pChassis  = &pDesign->GetChassis();
                m_state.pWeapon   = &pDesign->GetWeapon();
                m_state.pArmour   = &pDesign->GetArmour();
                m_state.pReactor  = &pDesign->GetReactor();
                m_state.pAbility1 = pDesign->GetAbility1();
                m_state.pAbility2 = pDesign->GetAbility2();
            }
        }
    ));

    // Left: unit status for the selected design
    m_elements.push_back(std::make_unique<UnitStatusPanel>(
        [this]() { return m_pSelectedDesign; },
        pUnitManager,
        statusLayout
    ));
}

bool UnitDesignerView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void UnitDesignerView::ShowComponentSelector_(
    UnitComponentType_t type,
    std::function<void(const UnitComponentConfig_t&)> onSelected
)
{
    std::vector<const UnitComponentConfig_t*> available;
    for (const auto& rConfig : m_rRegistry.GetAll())
    {
        if (rConfig.type == type)
        {
            available.push_back(&rConfig);
        }
    }

    m_elements.push_back(std::make_unique<ComponentSelectorPopup>(
        std::move(available),
        ResolveLayout(m_layout, k_SelectorPopupRatio),
        std::move(onSelected)
    ));
}

void UnitDesignerView::HandleSaveDesign_()
{
    if (!m_state.HasAllMandatory())
    {
        return;
    }

    m_rMilitary.AddDesign(std::make_unique<UnitDesign>(
        *m_state.pChassis,
        *m_state.pWeapon,
        *m_state.pArmour,
        *m_state.pReactor,
        m_state.pAbility1,
        m_state.pAbility2
    ));
}

} // namespace ac
