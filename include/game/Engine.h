#pragma once

#include <memory>
#include <string>

namespace ac {

class Graphics;
class Input;

class Engine {
public:
    Engine();
    ~Engine();
    void Run();

private:
    void Initialize_() const;
    void CheckInitialized_() const;
    void PrintWelcome_() const;

    std::unique_ptr<Graphics> m_graphics;
    std::unique_ptr<Input> m_input;
};

} // namespace ac
