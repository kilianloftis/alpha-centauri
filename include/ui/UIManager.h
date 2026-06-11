#pragma once

#include <functional>
#include <memory>
#include <string>

namespace ac
{

class Graphics;
class Input;
class UIWorldMap;
class UIPanel;
class UIPopup;

class UIManager
{
public:
    virtual ~UIManager() = default;

    virtual bool Initialize(Graphics& rGraphics) = 0;
    virtual void Draw(Graphics& rGraphics) = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void HandleInput(Input& rInput) = 0;

    virtual UIWorldMap& GetWorldMap() = 0;
    virtual UIPanel& GetInfoPanel() = 0;

    virtual void ShowPopup(const std::string& text, std::function<void()> onDismiss = nullptr) = 0;
    virtual void DismissPopup() = 0;
    virtual bool HasActivePopup() const = 0;
};

std::unique_ptr<UIManager> CreateUIManager();

} // namespace ac
