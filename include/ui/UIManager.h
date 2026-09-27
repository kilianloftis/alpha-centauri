#pragma once

#include "ui/HotkeyConfig.h"
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

    // Request a paint on the next Render(). Safe to call when nothing is dirty yet.
    void MarkFrameDirty();

    void RegisterViewShortcut(HotkeyChord_t chord, ViewFactory_t factory);
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
    void HandleGlobalShortcut_(const KeyEvent_t& rEvent);

    Graphics& m_rGraphics;
    Input& m_rInput;
    std::unique_ptr<IWorldView> m_pWorldView;
    std::vector<std::unique_ptr<IGameView>> m_overlayStack;
    std::unordered_map<HotkeyChord_t, ViewFactory_t, HotkeyChordHash> m_shortcutMap;
    bool m_bShouldExit = false;
    bool m_bFrameDirty = true;
};
} // namespace ac
