#include "ui/unit-designer/UnitDesignerView.h"
#include "ui/unit-designer/SlotColumnPanel.h"
#include "ui/unit-designer/ComponentSelectorPopup.h"
#include "ui/unit-designer/DesignStatsDisplay.h"
#include "ui/unit-designer/DesignListPanel.h"
#include "ui/unit-designer/UnitStatusPanel.h"
#include "game/faction/Military.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "game/units/UnitDesign.h"
#include "input/Input.h"

namespace ac
{

UnitDesignerView::UnitDesignerView(
    Military& rMilitary,
    const UnitComponentRegistry& rComponentRegistry,
    const UnitSlotRegistry& rSlotRegistry,
    const UnitManager* pUnitManager,
    WindowLayout_t layout
)
    : IGameView(layout)
    , m_rMilitary(rMilitary)
    , m_rComponentRegistry(rComponentRegistry)
    , m_rSlotRegistry(rSlotRegistry)
{
    std::vector<SlotColumnPanel::SlotEntry_t> leftSlots;
    std::vector<SlotColumnPanel::SlotEntry_t> rightSlots;

    for (const auto& rSlot : m_rSlotRegistry.GetAll())
    {
        SlotColumnPanel::SlotEntry_t entry;
        entry.pSlotConfig = &rSlot;
        entry.getComponent = [this, slotId = rSlot.id]() -> const UnitComponentConfig_t*
        {
            auto it = m_state.components.find(slotId);
            return it != m_state.components.end() ? it->second : nullptr;
        };
        entry.onClicked = [this, slotId = rSlot.id, compType = rSlot.componentType]()
        {
            ShowComponentSelector_(compType, [this, slotId](const UnitComponentConfig_t& rComp)
            {
                m_state.components[slotId] = &rComp;
            });
        };

        if (rSlot.column == "right")
        {
            rightSlots.push_back(std::move(entry));
        }
        else
        {
            leftSlots.push_back(std::move(entry));
        }
    }

    m_elements.push_back(std::make_unique<UnitStatusPanel>(
        [this]() { return m_pSelectedDesign; },
        pUnitManager,
        ResolveLayout(m_layout, k_BottomLeftPanelLayout)
    ));

    const WindowLayout_t topPanel = ResolveLayout(m_layout, k_TopPanelLayout);

    m_elements.push_back(std::make_unique<SlotColumnPanel>(
        std::move(leftSlots),
        ResolveLayout(topPanel, {0.0f, 0.0f, 0.2f, 1.0f})
    ));

    m_elements.push_back(std::make_unique<DesignStatsDisplay>(
        &m_state,
        &m_rSlotRegistry.GetAll(),
        ResolveLayout(topPanel, {0.2f, 0.0f, 0.6f, 1.0f}),
        [this]() { HandleSaveDesign_(); }
    ));

    m_elements.push_back(std::make_unique<SlotColumnPanel>(
        std::move(rightSlots),
        ResolveLayout(topPanel, {0.8f, 0.0f, 0.2f, 1.0f})
    ));

    m_elements.push_back(std::make_unique<DesignListPanel>(
        &m_rMilitary,
        ResolveLayout(m_layout, k_CenterPanelLayout),
        [this](const UnitDesign* pDesign)
        {
            m_pSelectedDesign = pDesign;
            if (pDesign)
            {
                for (const auto& rSlot : m_rSlotRegistry.GetAll())
                {
                    m_state.components[rSlot.id] = pDesign->GetComponentForSlot(rSlot.id);
                }
            }
        }
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
    const std::string& rComponentType,
    std::function<void(const UnitComponentConfig_t&)> onSelected
)
{
    std::vector<const UnitComponentConfig_t*> available;
    for (const auto& rConfig : m_rComponentRegistry.GetAll())
    {
        if (rConfig.type == rComponentType)
        {
            available.push_back(&rConfig);
        }
    }

    m_elements.push_back(std::make_unique<ComponentSelectorPopup>(
        std::move(available),
        ResolveLayout(m_layout, k_PopupLayoutSmall),
        std::move(onSelected)
    ));
}

void UnitDesignerView::HandleSaveDesign_()
{
    if (!m_state.HasAllMandatory(m_rSlotRegistry.GetAll()))
    {
        return;
    }

    const bool added = m_rMilitary.AddDesign(std::make_unique<UnitDesign>(
        m_rSlotRegistry.GetAll(),
        m_state.components
    ));
    if (!added)
    {
        // A design with this component combination already exists — no-op for now.
        // TODO: surface a status message in the UI once a notification system exists.
        return;
    }
}

} // namespace ac
