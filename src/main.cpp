#include <exception>
#include <iostream>
#include "game/Engine.h"

// Startup failures are reported, not recovered: every loader throws a message naming the file
// and key at fault, and an exception escaping main would lose it.
int main()
{
    try
    {
        ac::Engine engine;
        engine.Run();
    }
    catch (const std::exception& rError)
    {
        std::cerr << "Fatal error: " << rError.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
    return 0;
}
