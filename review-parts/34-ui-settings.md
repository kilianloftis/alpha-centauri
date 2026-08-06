## UI — settings

**Files:** `src/ui/settings/SettingsPanel.cpp`, `include/ui/settings/SettingsPanel.h`, `src/ui/settings/SettingsView.cpp`, `include/ui/settings/SettingsView.h`, `include/ui/settings/SettingDescriptor.h`

**Assessment:** This slice is small and mostly clear: `SettingsView` is a thin Escape-to-close overlay, and `SettingsPanel` drives rows from a static `SettingDescriptor_t` table with accessors into `GameSettings` rather than raw member pointers. The dominant weakness is that `SettingScope_t` and descriptor invariants are only half-enforced — click/render paths assume a correct table and treat `NewGameOnly` as “never editable,” which will silently break the first editable new-game bool.

### [M] Make `NewGameOnly` session-aware instead of permanently non-editable
`include/ui/settings/SettingDescriptor.h:10-15` documents scope as controlling editability “in the current session context,” but neither `SettingsView` nor `SettingsPanel` receives any new-game/in-progress flag. `HandleMouseClick` skips every `NewGameOnly` bool unconditionally (`SettingsPanel.cpp:185-188`), so a future editable new-game bool would never toggle. Pass a session flag into the panel (or view) and allow `NewGameOnly` edits only when that flag is set; until then, do not use `NewGameOnly` on `Bool` rows.

### [M] Ignore non-left clicks in `HandleMouseClick`
`SettingsPanel.cpp:176-198` toggles and `Save()`s on any mouse button that reaches the handler. Peer UI elements require `MouseButton_t::Left` before acting. Right/middle press currently flips preferences and writes `user_settings.json`. Gate the handler on left button before hit-testing rows.

### [M] Enforce descriptor kind/callback invariants before calling through
`SettingDescriptor_t` defaults callbacks to null (`SettingDescriptor.h:29-31`). `Render` and `HandleMouseClick` invoke `getBool` / `setBool` / `getValueText` with no checks (`SettingsPanel.cpp:160`, `165`, `195`). A mismatched table row is undefined behavior; project guidelines prefer throwing on unexpected null. After resolving `kind`, throw if the required callback(s) are null (and treat unknown kinds as errors rather than falling through to `getValueText`).

### [M] Stop treating every non-header/non-bool row as `ReadOnlyValue`
`SettingsPanel.cpp:157-170` branches `Bool` vs else; the else always concatenates `getValueText(...)`. Adding a new `SettingRowKind_t` without updating this switch will crash or mis-render. Switch on `kind` exhaustively (or `default:` throw) so new row types fail loudly at the panel, not at the function pointer.

### [L] Convention and hygiene items
- `include/ui/settings/SettingsView.h:18` / `SettingsView.cpp:11-15` — `m_rSettings` is only forwarded in the constructor; drop the member and pass the ctor parameter straight into `SettingsPanel` (same dead-store pattern as some other views, still noise here).
- `src/ui/settings/SettingsPanel.cpp:135` — `const auto& style` should be `rStyle` per reference naming.
- `src/ui/settings/SettingsPanel.cpp:17-20` — prefer `GameSettings::IsPauseAtEndOfTurn()` over reaching into `GetGameRules().pauseAtEndOfTurn`.
- `src/ui/settings/SettingsPanel.cpp:145-173` and `180-198` — duplicate row-count / `rowHeight` / `RowAt_` loops; extract one helper that yields `(descriptor, area)` to keep hit-testing aligned with paint.
- Implemented bool toggle + persist has no UI-level test coverage (only `GameSettings` unit tests exist).

**Observed outside slice:**
- `docs/architecture/ui-system.md` — Settings view/panel are absent from the UI architecture diagram and overview despite being a live `ViewFactory` product.
