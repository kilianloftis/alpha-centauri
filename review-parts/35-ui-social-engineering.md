## UI — social engineering

**Files:** `src/ui/social-engineering/SocialEngineeringBottomPanel.cpp`, `include/ui/social-engineering/SocialEngineeringBottomPanel.h`, `src/ui/social-engineering/SocialEngineeringDisplay.cpp`, `include/ui/social-engineering/SocialEngineeringDisplay.h`, `src/ui/social-engineering/SocialEngineeringView.cpp`, `include/ui/social-engineering/SocialEngineeringView.h`

**Assessment:** Policy grid layout, hit-testing, and active-policy mutation are coherent and share `GetPolicyCellLayout`, which keeps render and click paths aligned. Prior invented faction-rating logic and the fixed 4-column truncate are gone. The dominant weaknesses are a half-finished faction-bonus formatter that always shows `"None"`, and a bottom-panel research readout that uses the wrong Faction API for a “turns until breakthrough” label.

### [H] Finish `FormatFactionBonuses` — faction bonus line always shows "None"
`src/ui/social-engineering/SocialEngineeringDisplay.cpp:110-141` — The loop resolves each non-zero social rating, looks up `FindSocialRatingLevelEffects`, then discards `pLevelEffects` without writing to `oss` or clearing `first`. `Render` always draws `"None"` (`:455`) even when scores are non-zero and `config/social_rating_effects.json` has StatModifier/RuleFlag payloads. This is a wired, player-visible stub that silently lies. Format each level’s effects (at least StatModifier and RuleFlag) into `oss`, and throw (do not `continue`) when a known rating id is missing from the registry.

### [M] Bottom panel uses full-tech duration instead of remaining breakthrough turns
`src/ui/social-engineering/SocialEngineeringBottomPanel.cpp:87` — Label + `FormatTurnCount` present a turns-until-breakthrough figure, but the call is `GetBreakthroughRate()`, which `ResearchManager` documents as full turns ignoring accumulated progress. After any research progress the number is too high. Use `GetTurnsUntilBreakthrough()` instead.

### [M] Nullable deps deferred to Render; click path swallows null
`SocialEngineeringDisplay` / `BottomPanel` / `View` constructors accept `Faction*` / registry pointers with no validation (`SocialEngineeringDisplay.cpp:252-262`, `SocialEngineeringBottomPanel.cpp:32-38`, `SocialEngineeringView.cpp:10-31`). `Render` throws on null; `HandleMouseClick` silently returns (`SocialEngineeringDisplay.cpp:465-467`). Prefer references (or throw in the constructor) so a bad `ViewFactory` wiring fails at push time, not mid-frame / mid-click.

### [M] Hardcoded category and rating axis tables drift from enums
`SocialEngineeringDisplay.cpp:28-46` — `k_Categories` and `k_AllRatings` manually list every `SocialCategory_t` / `SocialRatingId_t`. A new axis in the enum + config will not appear in scores, hit-test loops, or the (intended) faction-bonus line until these arrays are edited. Drive iteration from `magic_enum::enum_values` (keep the one explicit display map for `Future Society`).

### [L] Convention and hygiene items
- `include/ui/social-engineering/SocialEngineeringView.h:25-27` / `SocialEngineeringView.cpp:17-19` — `m_pFaction`, `m_pPolicyRegistry`, `m_pRatingRegistry` are only forwarded in the constructor; dead members after child creation (unlike `ResearchView`, which still uses its pointer).
- `SocialEngineeringDisplay.cpp:147,154,373` — locals `isActive` should be `bIsActive` per boolean naming.
- `SocialEngineeringDisplay.cpp:78,116` — locals `first` should be `bFirst`.
- No tests cover SE UI formatting or click→`SetActivePolicy` (implemented paths); the always-`"None"` bonus line would have been an obvious case.

**Observed outside slice:**
- `docs/architecture/ui-system.md` — `SocialEngineeringView` / display / bottom panel are absent from the UI diagram and ViewFactory list (architecting rule: keep diagrams current).
- `src/ui/ViewFactory.cpp:96-100` — passes `socialPolicyRegistry.get()` / `socialRatingRegistry.get()` without null checks, so a missing registry becomes a late Render throw inside this slice.
