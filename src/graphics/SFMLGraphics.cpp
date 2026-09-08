#ifdef USE_SFML

#include "graphics/Graphics.h"
#include "input/KeyMapping.h"
#include "input/PlatformEventQueue.h"
#include <SFML/Graphics.hpp>
#include <SFML/System/Sleep.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#if defined(SFML_SYSTEM_WINDOWS)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(SFML_SYSTEM_LINUX) || defined(SFML_SYSTEM_FREEBSD) || defined(SFML_SYSTEM_OPENBSD) || \
    defined(SFML_SYSTEM_NETBSD)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
// Xlib macros collide with identifiers used elsewhere in this translation unit.
#undef None
#undef Status
#endif

namespace ac
{

namespace
{

constexpr int          k_MaximizeWaitAttempts = 50;
constexpr sf::Time     k_MaximizeWaitSlice    = sf::milliseconds(10);

// Ask the window manager to maximize. SFML 3.0 only exposes Windowed/Fullscreen states.
void MaximizeNativeWindow_(sf::WindowBase& rWindow)
{
#if defined(SFML_SYSTEM_WINDOWS)
    ShowWindow(rWindow.getNativeHandle(), SW_SHOWMAXIMIZED);
#elif defined(SFML_SYSTEM_LINUX) || defined(SFML_SYSTEM_FREEBSD) || defined(SFML_SYSTEM_OPENBSD) || \
    defined(SFML_SYSTEM_NETBSD)
    Display* pDisplay = XOpenDisplay(nullptr);
    if (!pDisplay)
    {
        return;
    }

    const Atom wmState = XInternAtom(pDisplay, "_NET_WM_STATE", False);
    const Atom maxHorz = XInternAtom(pDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    const Atom maxVert = XInternAtom(pDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    XEvent event{};
    event.xclient.type         = ClientMessage;
    event.xclient.window       = rWindow.getNativeHandle();
    event.xclient.message_type = wmState;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = 1; // _NET_WM_STATE_ADD
    event.xclient.data.l[1]    = static_cast<long>(maxHorz);
    event.xclient.data.l[2]    = static_cast<long>(maxVert);
    event.xclient.data.l[3]    = 1; // application

    XSendEvent(
        pDisplay,
        DefaultRootWindow(pDisplay),
        False,
        SubstructureRedirectMask | SubstructureNotifyMask,
        &event);
    XFlush(pDisplay);
    XCloseDisplay(pDisplay);
#else
    (void)rWindow;
#endif
}

class SFMLGraphics : public Graphics
{
public:
    SFMLGraphics(PlatformEventQueue& rEvents, const GraphicsConfig_t& rConfig)
        : m_rEvents(rEvents)
        , m_framerateLimit(rConfig.framerateLimit)
        , m_window(sf::VideoMode(sf::Vector2u(rConfig.windowWidth, rConfig.windowHeight)),
                   rConfig.windowTitle)
    {
        std::cout << "[SFMLGraphics] Creating window...\n";
        if (!m_window.isOpen())
        {
            throw std::runtime_error("[SFMLGraphics] Failed to create SFML render window");
        }

        const sf::Vector2u sizeBeforeMaximize = m_window.getSize();
        MaximizeNativeWindow_(m_window);
        // Maximize is applied asynchronously by the WM; wait briefly for the Resized event.
        for (int attempt = 0; attempt < k_MaximizeWaitAttempts; ++attempt)
        {
            PumpInto_();
            if (m_window.getSize() != sizeBeforeMaximize)
            {
                break;
            }
            sf::sleep(k_MaximizeWaitSlice);
        }

        m_window.setFramerateLimit(rConfig.framerateLimit);
        m_window.setKeyRepeatEnabled(false);
        m_window.requestFocus();
        std::cout << "[SFMLGraphics] Window created.\n";

        LoadFont_(rConfig.fontPaths);
    }

    void PumpEvents() override
    {
        PumpInto_();
    }

    void Clear() override
    {
        m_window.clear(sf::Color::Black);
    }

    void Display() override
    {
        m_window.display();
        m_lastPace = std::chrono::steady_clock::now();
    }

    void PaceFrame() override
    {
        if (m_framerateLimit == 0)
        {
            return;
        }
        const auto frame = std::chrono::microseconds(1000000 / m_framerateLimit);
        const auto now = std::chrono::steady_clock::now();
        const auto nextFrame = m_lastPace + frame;
        if (now < nextFrame)
        {
            std::this_thread::sleep_for(nextFrame - now);
        }
        m_lastPace = std::chrono::steady_clock::now();
    }

    bool LoadTexture(const std::string& id, const std::string& path) override
    {
        sf::Texture texture;
        if (!texture.loadFromFile(path))
        {
            std::cerr << "[Graphics] Failed to load texture '" << path << "'.\n";
            return false;
        }
        // Assign, so reloading an id replaces the GPU data rather than keeping the first copy.
        m_textures.insert_or_assign(id, std::move(texture));
        return true;
    }

    bool UpsertTextureRGBA(const std::string& id, unsigned int width, unsigned int height,
                           const std::uint8_t* rgba) override
    {
        if (!rgba || width == 0 || height == 0)
        {
            return false;
        }

        const sf::Vector2u size{width, height};
        auto it = m_textures.find(id);
        if (it == m_textures.end())
        {
            sf::Texture texture;
            if (!texture.resize(size))
            {
                std::cerr << "[Graphics] Failed to create texture '" << id << "'.\n";
                return false;
            }
            texture.setSmooth(false);
            texture.update(rgba);
            m_textures.emplace(id, std::move(texture));
            return true;
        }

        sf::Texture& rTexture = it->second;
        if (rTexture.getSize() != size)
        {
            if (!rTexture.resize(size))
            {
                std::cerr << "[Graphics] Failed to resize texture '" << id << "'.\n";
                return false;
            }
        }
        rTexture.setSmooth(false);
        rTexture.update(rgba);
        return true;
    }

    bool DrawSprite(const std::string& textureId, float x, float y) override
    {
        auto it = m_textures.find(textureId);
        if (it == m_textures.end())
        {
            std::cerr << "[Graphics] Texture '" << textureId << "' is not loaded.\n";
            return false;
        }

        sf::Sprite sprite(it->second);
        sprite.setPosition({x, y});
        m_window.draw(sprite);
        return true;
    }

    bool DrawSprite(const std::string& textureId, float x, float y, float destWidth,
                    float destHeight) override
    {
        auto it = m_textures.find(textureId);
        if (it == m_textures.end())
        {
            std::cerr << "[Graphics] Texture '" << textureId << "' is not loaded.\n";
            return false;
        }

        const sf::Vector2u size = it->second.getSize();
        if (size.x == 0 || size.y == 0)
        {
            return false;
        }

        sf::Sprite sprite(it->second);
        sprite.setPosition({x, y});
        sprite.setScale({destWidth / static_cast<float>(size.x),
                         destHeight / static_cast<float>(size.y)});
        m_window.draw(sprite);
        return true;
    }

    void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color_t& color = Color_t::White()) override
    {
        sf::Text drawable(m_font, text, size);
        drawable.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
        drawable.setPosition({x, y});
        m_window.draw(drawable);
    }

    void DrawRect(float x, float y, float width, float height, const Color_t& color, float thickness) override
    {
        sf::RectangleShape rect(sf::Vector2f(width, height));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(sf::Color::Transparent);
        rect.setOutlineColor(sf::Color(color.r, color.g, color.b, color.a));
        rect.setOutlineThickness(thickness);
        m_window.draw(rect);
    }

    void DrawFilledRect(float x, float y, float width, float height, const Color_t& color) override
    {
        sf::RectangleShape rect(sf::Vector2f(width, height));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
        m_window.draw(rect);
    }

    void DrawLine(float x1, float y1, float x2, float y2, const Color_t& color, float thickness) override
    {
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0f || thickness <= 0.0f)
        {
            return;
        }

        sf::RectangleShape line(sf::Vector2f(length, thickness));
        line.setOrigin({0.f, thickness * 0.5f});
        line.setPosition({x1, y1});
        line.setRotation(sf::radians(std::atan2(dy, dx)));
        line.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
        m_window.draw(line);
    }

    unsigned int GetWindowWidth() const override
    {
        return m_window.getSize().x;
    }

    unsigned int GetWindowHeight() const override
    {
        return m_window.getSize().y;
    }

private:
    // Non-virtual, because the constructor's maximize wait pumps too and a virtual call there
    // would not reach an override.
    void PumpInto_()
    {
        while (auto event = m_window.pollEvent())
        {
            DispatchEvent_(*event);
        }
    }

    void LoadFont_(const std::vector<std::string>& rFontPaths)
    {
        for (const std::string& rPath : rFontPaths)
        {
            if (m_font.openFromFile(rPath))
            {
                return;
            }
        }

        // The whole UI is text and rectangles, so "no font" is not a state this backend can
        // usefully run in: it would present a black window with no diagnostic.
        std::string tried;
        for (const std::string& rPath : rFontPaths)
        {
            tried += "\n  " + rPath;
        }
        throw std::runtime_error("[SFMLGraphics] No usable font. Tried:"
                                 + (tried.empty() ? std::string(" (none configured)") : tried));
    }

    void DispatchEvent_(const sf::Event& rEvent)
    {
        if (rEvent.is<sf::Event::Closed>())
        {
            // Recorded, not decided: what a close request means belongs to the engine.
            m_rEvents.RequestClose();
            return;
        }

        if (const auto* pResized = rEvent.getIf<sf::Event::Resized>())
        {
            m_window.setView(sf::View(sf::FloatRect(
                {0.f, 0.f},
                {static_cast<float>(pResized->size.x), static_cast<float>(pResized->size.y)})));
            return;
        }

        if (const auto* pKey = rEvent.getIf<sf::Event::KeyPressed>())
        {
            if (auto mapped = KeyFromSfKey(pKey->code))
            {
                m_rEvents.PushKey({*mapped, GetModifierState()});
            }
            return;
        }

        if (const auto* pMouse = rEvent.getIf<sf::Event::MouseButtonPressed>())
        {
            PushMouseButton_(*pMouse, /*bPressed=*/true);
            return;
        }

        if (const auto* pMouse = rEvent.getIf<sf::Event::MouseButtonReleased>())
        {
            PushMouseButton_(*pMouse, /*bPressed=*/false);
            return;
        }

        if (const auto* pMoved = rEvent.getIf<sf::Event::MouseMoved>())
        {
            m_rEvents.PushMouse({MouseButton_t::None, static_cast<int>(pMoved->position.x),
                                 static_cast<int>(pMoved->position.y), {}, false});
        }
    }

    template <typename TMouseEvent>
    void PushMouseButton_(const TMouseEvent& rEvent, bool bPressed)
    {
        if (auto mapped = MouseButtonFromSfButton(rEvent.button))
        {
            m_rEvents.PushMouse({*mapped, static_cast<int>(rEvent.position.x),
                                 static_cast<int>(rEvent.position.y), GetModifierState(),
                                 bPressed});
        }
    }

    PlatformEventQueue& m_rEvents;
    unsigned int m_framerateLimit = 60;
    sf::RenderWindow m_window;
    sf::Font m_font;
    std::unordered_map<std::string, sf::Texture> m_textures;
    std::chrono::steady_clock::time_point m_lastPace = std::chrono::steady_clock::now();
};

} // namespace

std::unique_ptr<Graphics> CreateGraphics(PlatformEventQueue& rEvents,
                                         const GraphicsConfig_t& rConfig)
{
    return std::make_unique<SFMLGraphics>(rEvents, rConfig);
}

} // namespace ac

#endif // USE_SFML
