#pragma once

#include "ui/UIElement.h"
#include "ui/IGameView.h"
#include "ui/base/BaseView.h"
#include "ui/research/ResearchView.h"
#include "ui/world/WorldView.h"
#include <functional>
#include <memory>

namespace ac
{

class GameState;
class GameDataContext;
class Graphics;
class BaseManager;

class ViewFactory
{
public:
    ViewFactory(
        GameState& rGameState,
        GameDataContext& rGameDataContext,
        Graphics& rGraphics
    );

    std::unique_ptr<WorldView> CreateWorldView(
        const WindowLayout_t& layout,
        std::function<void()> onProcessTurn,
        std::function<void()> onRequestExit,
        std::function<void(BaseManager&)> onOpenBase
    ) const;

    std::unique_ptr<BaseView> CreateBaseView(BaseManager& rBase) const;

    std::unique_ptr<BaseView> CreateBaseView(
        BaseManager& rBase,
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<ResearchView> CreateResearchView(
        const WindowLayout_t& layout
    ) const;

    WindowLayout_t GetFullscreenLayout() const;

private:
    GameState& m_rGameState;
    GameDataContext& m_rGameDataContext;
    Graphics& m_rGraphics;
};

} // namespace ac
