#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;
struct BaseDisplaySnapshot_t;

class GrowthDisplay : public UIElement
{
public:
    // rSnapshot is the view's per-change snapshot; it outlives every panel it feeds.
    GrowthDisplay(
        const BaseManager& rBase,
        const BaseDisplaySnapshot_t& rSnapshot,
        WindowLayout_t layout
    );
    ~GrowthDisplay() override = default;

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager& m_rBase;
    const BaseDisplaySnapshot_t& m_rSnapshot;
};

} // namespace ac
