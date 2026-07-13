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
Throw errors if expected pointers are null
Do not keep old code for legacy of backwards compatibilty reasons. Update all call sites as needed.

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
- **Classes**: PascalCase (e.g., `GameEngine`, `GraphicsSystem`). Never postfix classes with `_t`.
- **Data structs** (config, POD, DTOs): PascalCase + `_t` (e.g., `KeyEvent_t`, `SocialPolicyConfig_t`).
- **Event structs**: `Ev` prefix is the type marker (e.g., `EvTurnStarted`); do not also add `_t`.
- **Enum classes**: PascalCase + `_t` (e.g., `StatId_t`, `GameCategory_t`, `Key_t`). Always — including names that already end in `Id`/`Kind`/`Op`.
- **Constants**: `k_` prefix + PascalCase remainder (e.g., `k_MaxPlayers`, `k_FullscreenLayout`). Applies to `constexpr` / `const` values at namespace or class scope.
- **Parser classes** for a `Foo_t` struct: name them `FooParser`, never `Foo_tParser`.
- **Functions / methods**: PascalCase (e.g., `Initialize()`, `EventBus::Subscribe`, `Signal::Connect` / `Emit`).
- **Public Signal members**: PascalCase with an `On` prefix (e.g., `OnGrowth`, `OnPopGained`).
- **Private methods**: trailing underscore (e.g., `HandleClick_`).
- **Variables**: camelCase (e.g., `playerPosition`, `frameCount`).
- **Boolean variables**: Prefix with 'b' (e.g., `bIsActive`, `bHasCompleted`).
- **Pointers**: prefix with 'p' (e.g., `pEngine`, `pGraphics`).
- **References**: prefix with 'r' (e.g., `rEngine`, `rGraphics`).
- **Member variables**: prefix with 'm_' then camelCase (e.g., `m_engine`, `m_pGraphics`, `m_goldenAge`).

### Enum ↔ string
- When the wire/config form matches the enumerator name (possibly only by case), use `magic_enum` rather than a hand-rolled switch.
- When the wire form differs (e.g. `StatId_t::HitPoints` ↔ `"hit_points"`, or a display label with spaces), keep **one** explicit map next to the enum — do not duplicate maps across call sites.

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
