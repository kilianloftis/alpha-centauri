#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"
#include <vector>

namespace ac
{

class Graphics;

class UIGroup : public UIElement
{
public:
virtual ~UIGroup() = default;

void Render() override
{
    for (const auto& element : m_elements) {
        element->Render();
    }
}
void Update() override
{
    for (const auto& element : m_elements) {
        element->Update();
    }
}

virtual void HandleKey(const KeyEvent_t& rEvent) {
    for (const auto& element : m_elements) {
        element->HandleKey(rEvent);
    }
}

void HandleMouse(const MouseEvent_t& rEvent) {
    for (const auto& element : m_elements) {
        if (element->Contains(rEvent.x, rEvent.y)) {
            element->HandleMouseClick(rEvent);
            break;
        }
    }
}

bool Contains(float x, float y) const {
    for (const auto& element : m_elements) {
        if (element->Contains(x, y)) {
            return true;
        }
    }
    return false;
}

protected:
    std::vector<std::unique_ptr<UIElement>> m_elements;
};

} // namespace ac
