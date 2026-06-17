#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class Graphics;
class Pop;

// Individual pop button that can be clicked to change assignment
class PopButton : public UIElement
{
public:
    using OnClickCallback_t = std::function<void(const Pop&)>;

    PopButton(const Graphics& rGraphics, const Pop& rPop, ResolvedLayout_t layout, OnClickCallback_t onClick);
    ~PopButton() override = default;

    void Render() override;
    void Update() override {}
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    const Pop& m_rPop;
    OnClickCallback_t m_onClick;
};

} // namespace ac
