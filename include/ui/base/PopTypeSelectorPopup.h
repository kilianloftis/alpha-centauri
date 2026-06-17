#pragma once

#include "ui/UIPopup.h"
#include <functional>
#include <vector>

namespace ac
{

class Faction;
class Graphics;
struct PopTypeConfig;

// Popup that lists the pop types currently available for a faction.
// "Available" means: bPlayerAssignable is true, and requiredTech is either
// empty or has been discovered by the faction.
class PopTypeSelectorPopup : public UIPopup
{
public:
    PopTypeSelectorPopup(
        const Faction& rFaction,
        Graphics& rGraphics,
        PanelLayout_t layout,
        std::function<void(const PopTypeConfig&)> onPopTypeSelected
    );

    ~PopTypeSelectorPopup() override = default;

    void Render() override;
    void Update() override {}

    void HandleMouseClick(const MouseEvent_t& rEvent) override;

    // Returns pop type configs visible to the player given current research.
    std::vector<const PopTypeConfig*> GetAvailablePopTypes() const;

private:
    static constexpr float kHeaderFontSizeRatio = 0.04f;
    static constexpr float kEntryFontSizeRatio  = 0.03f;
    static constexpr float kLineHeightRatio     = 0.05f;
    static constexpr float kPaddingRatio        = 0.02f;

    const Faction& m_rFaction;
    std::function<void(const PopTypeConfig&)> m_onPopTypeSelected;
};

} // namespace ac
