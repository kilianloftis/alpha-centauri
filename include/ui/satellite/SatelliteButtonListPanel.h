#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class SatelliteLabeledButton;

// Vertical list of fixed-size selectable buttons (scrollbar deferred). Selection is mutually
// exclusive within this panel.
class SatelliteButtonListPanel : public UIElement
{
public:
    struct Item_t
    {
        std::string id;
        std::string label;
    };

    SatelliteButtonListPanel(WindowLayout_t layout,
                             std::string header,
                             std::vector<Item_t> items,
                             std::optional<std::string> selectedId,
                             std::function<void(const std::string&)> onSelect);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    // Selection is mutually exclusive within this panel, and it is this panel's own state.
    // It was only ever honoured because the parent view tore the whole UI down and rebuilt it
    // with a new selectedId; the panel itself never updated anything.
    void SetSelected(const std::optional<std::string>& rSelectedId);

    // Replace the list contents (the target list depends on which faction is selected).
    void SetItems(std::vector<Item_t> items, const std::optional<std::string>& rSelectedId);

private:
    void RebuildButtons_();

    std::string m_header;
    std::vector<Item_t> m_items;
    std::optional<std::string> m_selectedId;
    std::function<void(const std::string&)> m_onSelect;
    std::vector<std::unique_ptr<SatelliteLabeledButton>> m_buttons;
};

} // namespace ac
