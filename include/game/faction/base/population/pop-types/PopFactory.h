#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ac
{

class Pop;
class PopTypeRegistry;

// PopFactory handles creation of individual Pop instances from config.
class PopFactory
{
public:
    PopFactory();
    ~PopFactory();

    // Inject the registry used to look up pop type definitions
    void SetRegistry(const PopTypeRegistry* pRegistry);

    // Create a pop of the given type id. Defaults to "Worker" if typeId is empty.
    // Returns nullptr if the id is not found in the registry.
    std::unique_ptr<Pop> CreatePop(const std::string& typeId = "Worker") const;

private:
    const PopTypeRegistry* m_pRegistry = nullptr;
};

} // namespace ac
