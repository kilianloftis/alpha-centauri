# Project Context and Component State

## Purpose

This skill helps you work efficiently on the Alpha Centauri project by describing the current state and architecture of each existing component.

- **Focus on what exists now** for each component
- **Capture architecture and implementation structure**
- **Avoid tracking change diaries or future plans here**

Use the component state notes in `./memories/repo/` as a snapshot of the current system and how the pieces fit together.

## When to use this skill

- **Start of session**: Read relevant component notes to understand the current system
- **Before modifying a component**: Review its current state and architecture
- **For integration work**: Compare state across existing components
- **When summarizing the project**: Record accurate, up-to-date architecture for each existing component

## Component files in /memories/repo/

### Core component notes

- **progress-diary.md** - Current overall project status and major architectural context
- **engine-diary.md** - Game loop architecture, state management, initialization
- **gameplay-diary.md** - Gameplay systems, rules, and interactions currently implemented
- **world-diary.md** - World generation, map structures, terrain, factions, cities, units
- **ai-diary.md** - AI opponent behavior, decision-making, and current pathfinding approach

### Note structure

Each component note should cover:

```
## Current State
[What exists now: implemented features, active modules, and current behavior]

## Architecture
[Current design, class/module structure, key data flows, and relationships]

## Dependencies
[How this component uses or connects to existing components]
```

## How to use these notes

1. **When starting work on a component**: Read its note first
2. **During implementation**: Keep the note aligned with the actual current state
3. **When exploring design**: Use the architecture section to understand interactions
4. **When connecting components**: Reference dependency information in existing notes

## Important guidance

- Do not use these notes to track future plans or roadmap items
- Do not invent or document components that do not already exist
- Keep the focus on the present architecture and current implementation state
- Store future development ideas and planning elsewhere
