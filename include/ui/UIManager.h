#pragma once

#include "ui/IGameView.h"
#include <memory>
#include <vector>

namespace ac
{

class Graphics;
class Input;

class UIManager
{
public:
    bool Initialize(Graphics& rGraphics, Input& rInput);
    void Update();
    void ProcessInput();
    void Render();

    void PushView(std::unique_ptr<IGameView> pView);
    void PopView();
    bool HasViews() const;

    bool ShouldExit() const;
    void RequestExit();

private:
    void ProcessKeys_();
    void ProcessMouse_();

    Graphics* m_pGraphics = nullptr;
    Input* m_pInput = nullptr;
    std::vector<std::unique_ptr<IGameView>> m_viewStack;
    bool m_bShouldExit = false;
};
} // namespace ac
