#pragma once

#include "ui/IGameView.h"
#include <memory>
#include <vector>

namespace ac
{

class Graphics;
class Input;
class WorldView;

class UIManager
{
public:
    using ViewFactory_t = std::function<std::unique_ptr<IGameView>()>;

    UIManager(Graphics& rGraphics, Input& rInput);

    void ProcessInput();
    void Render();

    void RegisterViewShortcut(Key_t key, ViewFactory_t factory);
    void SetWorldView(std::unique_ptr<WorldView> pWorldView);
    void PushView(std::unique_ptr<IGameView> pView);
    void PopView();
    bool HasViews() const;

    bool ShouldExit() const;
    void RequestExit();

private:
    void ProcessKeys_();
    void ProcessMouse_();
    IGameView* GetActiveView_();
    void HandleGlobalShortcut_(Key_t key);

    Graphics& m_rGraphics;
    Input& m_rInput;
    std::unique_ptr<WorldView> m_pWorldView;
    std::vector<std::unique_ptr<IGameView>> m_overlayStack;
    std::unordered_map<Key_t, ViewFactory_t> m_shortcutMap;
    bool m_bShouldExit = false;
};
} // namespace ac
