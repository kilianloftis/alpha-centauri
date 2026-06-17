#pragma once

#include "ui/UIGroup.h"
#include "ui/research/ResearchDisplay.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

class UIManager;

class ResearchView : public UIGroup
{
public:
    static constexpr float kWindowWidth = 600.f;
    static constexpr float kWindowHeight = 400.f;

    explicit ResearchView(UIManager& rUIManager);

    void Render() override;
    void Update(float deltaTime) override;
    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

    void OnPushed() override;
    void OnPopped() override;

    // Research information management
    const std::string& GetCurrentResearchTarget() const { return m_currentResearchTarget; }
    void SetCurrentResearchTarget(const std::string& target) { m_currentResearchTarget = target; }

private:
    UIManager& m_rUIManager;
    std::unique_ptr<ResearchDisplay> m_pDisplay;
    std::string m_currentResearchTarget;
};

} // namespace ac
