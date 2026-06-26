---
trigger: always_on
description: Coding guidelines for the project
---

# Coding Guidelines

Do not make up game rules or mechanics. Leave TODOs instead.
Only create functions if they are needed for the current implementation. Do not add functinos, getters, or setters, without an immediate requirement.
Use references whenever possible.
Use smart pointers if you are unable to use a reference.
Use range-based for loops whenever possible.
Constructors should accept all arguments required to make the object valid
If a class owns a resource, if possible, the class should construct that resource itself
Prefer throwing exceptions over returning default values

## SOLID Principles
All code must adhere to SOLID principles:
- **S**ingle Responsibility Principle: Each class/function should have one reason to change
- **O**pen/Closed Principle: Open for extension, closed for modification
- **L**iskov Substitution Principle: Derived classes must be substitutable for base classes
- **I**nterface Segregation Principle: Clients shouldn't depend on interfaces they don't use
- **D**ependency Inversion Principle: Depend on abstractions, not concretions

## Moddability and Customization
All code should be written with moddability and customization in mind:
- Use configuration files or parameters for customizable behavior
- Provide hooks for lua or binary plugins

## Code Formatting

### Naming Conventions
- **Classes**: PascalCase (e.g., `GameEngine`, `GraphicsSystem`)
- **Functions**: PascalCase (e.g., `Initialize()`, `RenderFrame()`)
- **Variables**: camelCase (e.g., `playerPosition`, `frameCount`)
- **Boolean variables**: Prefix with 'b' (e.g., `bIsActive`, `bHasCompleted`)
- **Pointers**: prefix with 'p' (e.g., `pEngine`, `pGraphics`)
- **References**: prefix with 'r' (e.g., `rEngine`, `rGraphics`)
- **Member variables**: prefix with 'm_' (e.g., `m_engine`, `m_pGraphics`)
- **Structs and Enums**: postfix with _t (e.g., `KeyEvent_t`, `MouseEvent_t`)

### Brace Style
Opening braces should be on their own line:
```cpp
void ExampleFunction(std::unique_ptr<SomeClass> pSomeClass)
{
    if (bCondition)
    {
        // code here
    }
}
```
