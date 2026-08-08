#pragma once

#include "ui/IGameView.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ac
{

class Graphics;
class Input;
class IWorldView;

class UIManager
{
public:
    using ViewFactory_t = std::function<std::unique_ptr<IGameView>()>;

    UIManager(Graphics& rGraphics, Input& rInput);
    ~UIManager();

    void ProcessInput();
    // Consumes UI-queued, non-input-driven turn advance requests (e.g. WorldView's auto
    // end-turn once no units need orders). Called once per frame between ProcessInput and
    // Render so turn advance never runs from the paint path.
    void Update();
    void Render();

    void RegisterViewShortcut(Key_t key, ViewFactory_t factory);
    void SetWorldView(std::unique_ptr<IWorldView> pWorldView);
    void PushView(std::unique_ptr<IGameView> pView);
    void PopView();
    bool HasOverlayView() const;

    // False while an overlay is on the stack, or the world view reports a blocking in-view
    // modal (probe/supply popup, ...). Engine::ProcessTurn_ soft-gates TurnProcessor::Advance
    // on this instead of throwing for an ordinary UI-initiated End Turn under a modal.
    bool CanAdvanceTurn() const;

    bool ShouldExit() const;
    void RequestExit();

private:
    void PruneClosedViews_();
    void ProcessKeys_();
    void ProcessMouse_();
    IGameView* GetActiveView_();
    void HandleGlobalShortcut_(Key_t key);

    Graphics& m_rGraphics;
    Input& m_rInput;
    std::unique_ptr<IWorldView> m_pWorldView;
    std::vector<std::unique_ptr<IGameView>> m_overlayStack;
    std::unordered_map<Key_t, ViewFactory_t> m_shortcutMap;
    bool m_bShouldExit = false;
};
} // namespace ac
