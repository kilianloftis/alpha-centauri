#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;

// Vertical list of this base's constructed buildings, plus buildings granted here by a
// continuous GrantBuilding effect. Granted rows use a darker colour; a constructed copy
// that is also granted is prefixed with "*".
class BuildingsDisplay : public UIElement
{
public:
    BuildingsDisplay(const BaseManager& rBase, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager& m_rBase;
};

} // namespace ac
