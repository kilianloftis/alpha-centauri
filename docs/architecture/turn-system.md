# Turn System Architecture

```mermaid
graph TB
    subgraph "Turn Processing"
        TurnProcessor[TurnProcessor]
        ProcessTurn[ProcessTurn()]
        ExecuteStage[ExecuteStage()]
        ProcessFactionTurn[ProcessFactionTurn()]
    end

    subgraph "TurnProcessor Methods"
        RunFactionProcessing[RunFactionProcessing()]
        HandlePlayerActions[HandlePlayerActions()]
        PostTurnProcessing[PostTurnProcessing()]
        ApplyWorldEffects[ApplyWorldEffects()]
        IncrementMissionYear[IncrementMissionYear()]
    end

    subgraph "Hook System"
        HookSystem[HookSystem]
        LoadConfig[LoadConfig()]
        ExecuteHooks[ExecuteHooks()]
        HasReplaceHook[HasReplaceHook()]
        GetStages[GetStages()]
    end

    subgraph "Data Structures"
        StageConfig[StageConfig<br/>id, name, description<br/>phase, order<br/>repeat_for_each_faction<br/>hooks, sub_stages]
        StageHooks[StageHooks<br/>pre, post, replace]
        Hook[Hook<br/>mod_id, script_path<br/>callback]
    end

    subgraph "Storage"
        StagesVector[vector<StageConfig><br/>m_stages]
        HooksMap[unordered_map<string, StageHooks><br/>m_stageHooksMap]
    end

    subgraph "Configuration"
        ConfigFile[config/turn_stages.json]
        JSONParser[nlohmann::json]
    end

    subgraph "Turn Stages"
        BeginTurn[begin_turn]
        FactionTurns[faction_turns]
        WorldEffects[world_effects]
        EndTurn[end_turn]
        FactionBegin[faction_begin]
        Production[production]
        Research[research]
        Population[population]
        PlayerActions[player_actions]
        AIActions[ai_actions]
        FactionEnd[faction_end]
        CollectIncome[collect_income]
    end

    TurnProcessor --> HookSystem
    TurnProcessor --> ProcessTurn
    TurnProcessor --> ExecuteStage
    TurnProcessor --> ProcessFactionTurn

    ProcessTurn --> ExecuteStage
    ExecuteStage --> ProcessFactionTurn

    ExecuteStage --> RunFactionProcessing
    ExecuteStage --> HandlePlayerActions
    ExecuteStage --> PostTurnProcessing
    ExecuteStage --> ApplyWorldEffects
    ExecuteStage --> IncrementMissionYear

    TurnProcessor --> HookSystem
    HookSystem --> LoadConfig
    HookSystem --> ExecuteHooks
    HookSystem --> HasReplaceHook
    HookSystem --> GetStages

    HookSystem --> StageConfig
    HookSystem --> StageHooks
    HookSystem --> Hook

    HookSystem --> StagesVector
    HookSystem --> HooksMap

    LoadConfig --> ConfigFile
    LoadConfig --> JSONParser

    StageConfig --> TurnStages

    style TurnProcessor fill:#bbf,stroke:#333,stroke-width:4px
    style HookSystem fill:#bbf,stroke:#333,stroke-width:4px
    style StageConfig fill:#ff9,stroke:#333,stroke-width:2px
    style ConfigFile fill:#bfb,stroke:#333,stroke-width:2px
```

## Component Overview

### TurnProcessor
- **Purpose**: Manages turn-based game logic and coordinates turn stages
- **Dependencies**: Requires shared_ptr to HookSystem
- **State**:
  - `m_hookSystem`: Shared pointer to HookSystem
  - `m_missionYear`: Current mission year
- **Methods**:
  - `ProcessTurn(missionYear, numFactions)`: Main entry point for turn processing
  - `ExecuteStage(stage, factionIndex)`: Executes a single stage with hooks
  - `ProcessFactionTurn(factionIndex)`: Processes a single faction's turn
  - `RunFactionProcessing(factionIndex)`: Runs faction-specific processing
  - `HandlePlayerActions(factionIndex)`: Handles player input/actions
  - `PostTurnProcessing(factionIndex)`: Post-turn cleanup
  - `ApplyWorldEffects()`: Applies world-wide effects
  - `IncrementMissionYear()`: Increments the mission year counter
- **Behavior**:
  - Iterates through stages from HookSystem
  - Executes stages once or per faction based on `repeat_for_each_faction` flag
  - Executes pre/post hooks around stage logic
  - Skips default implementation if replace hook exists
  - Recursively executes sub-stages

### HookSystem
- **Purpose**: Manages modding hooks and turn stage configuration
- **State**:
  - `m_stages`: Vector of StageConfig (ordered turn stages)
  - `m_stageHooksMap`: Map from stage ID to StageHooks
- **Methods**:
  - `LoadConfig(configPath)`: Loads turn stages and hooks from JSON file
  - `ExecuteHooks(stageId, hookType)`: Executes hooks of specified type (pre/post/replace)
  - `HasReplaceHook(stageId)`: Checks if stage has replace hooks
  - `GetStages()`: Returns const reference to stages vector
- **Behavior**:
  - Parses JSON configuration file
  - Builds hierarchical stage structure with sub-stages
  - Registers hooks for each stage
  - Executes hooks in order with callback invocation
  - Supports three hook types: pre, post, replace

### Data Structures

#### StageConfig
- **Purpose**: Configuration for a single turn stage
- **Fields**:
  - `id`: Unique stage identifier
  - `name`: Human-readable stage name
  - `description`: Stage description
  - `phase`: Phase grouping (e.g., "turn", "faction")
  - `order`: Execution order within phase
  - `repeat_for_each_faction`: Whether to repeat for each faction
  - `hooks`: StageHooks for this stage
  - `sub_stages`: Nested child stages

#### StageHooks
- **Purpose**: Container for hook lists
- **Fields**:
  - `pre`: Hooks to execute before stage logic
  - `post`: Hooks to execute after stage logic
  - `replace`: Hooks that replace stage logic

#### Hook
- **Purpose**: Single hook definition
- **Fields**:
  - `mod_id`: Mod identifier
  - `script_path`: Path to script file
  - `callback`: Function to execute (currently placeholder)

### Configuration
- **File**: `config/turn_stages.json`
- **Format**: JSON with `turn_processing.stages` array
- **Content**: Defines turn stages, their order, phases, and hooks
- **Parser**: Uses nlohmann::json library
- **Structure**: Supports hierarchical stages with sub-stages

### Turn Stages
The system supports the following stage IDs:
- `begin_turn`: Turn initialization
- `faction_turns`: Container for faction-specific stages
- `world_effects`: Apply world-wide effects
- `end_turn`: Turn finalization
- `faction_begin`: Faction turn start
- `production`: Production phase
- `research`: Research phase
- `population`: Population phase
- `player_actions`: Player input handling
- `ai_actions`: AI decision making
- `faction_end`: Faction turn end
- `collect_income`: Income collection

### Integration Flow
1. Engine creates TurnProcessor with shared HookSystem
2. Engine calls HookSystem.LoadConfig() during initialization
3. Engine calls TurnProcessor.ProcessTurn() each turn
4. TurnProcessor iterates stages from HookSystem
5. For each stage:
   - Check for replace hook
   - Execute pre hooks
   - Execute stage logic (if not replaced)
   - Execute sub-stages recursively
   - Execute post hooks
6. TurnProcessor updates mission year
