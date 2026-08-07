# Package 14 — UI: shared components and config-driven content

Findings re-verified against the tree at `a3305bf`.

## Scope decision, stated up front

This package lists five `[H]` and twelve `[M]`. Four of the `[H]` and all but one `[M]` are
implemented here. **The fifth — "Stop growing a process-global god-object style registry" — is
deferred to its own package**, and that is a judgement call the reader should be able to overrule.

The reasoning: `UiStyle` has 39 nested style structs, 39 `root.at(...)` lines in `Load`, and
**290 `Style()` call sites across 61 files**. Converting those to constructor-injected style
references is larger than everything else in this package combined, and it lands in rendering
code with no automated coverage — nothing in the test suite draws a frame. Bundling it with four
other `[H]` and eleven `[M]` maximises exactly the failure mode that has produced a review-caught
regression in eight of the last thirteen packages.

The two *narrow* style findings that can be fixed without touching call sites — the duplicate
type pairs and the misplaced elevation range — **are** done here.

## Verified diagnoses

### [H] Seven copies of one list-selector widget, already diverging

Verified by structural diff. `PopTypeSelectorPopup.cpp` and `ProductionSelectorPopup.cpp` are the
same widget modulo payload type, header string, empty-list string, and how a label is read.
`ProbeActionPopup`, `SupplyCrawlPopup` and `ComponentSelectorPopup` are the same shape again —
all five share `Style().productionSelectorPopup`, the same `CacheEntryRects_`, the same Escape
handling.

The drift the review predicted is present: `ProductionSelectorPopup.cpp:92-96` closes on an
outside click and null-checks the entry before dereferencing; `PopTypeSelectorPopup.cpp:95-112`
does neither.

**One thing has changed since the review:** it recorded outside-click dismiss as *unreachable*
because `IGameView::HandleMouse` only delivered clicks after a `Contains` test. Package 2 replaced
that with exclusive modal routing (`IGameView.h:63-70`: "a modal element captures every press,
including outside its own `Contains` rect"). So the branch is now live, and
`PopTypeSelectorPopup`'s *lack* of it is a real defect rather than dead code on both sides.

**Chosen:** one `ListSelectorPopup` taking a title, an empty-list message, a vector of row labels,
and an `onSelected(size_t index)` callback. Callers keep their own payload vector and index into
it. Not a template: one TU, one place where dismiss, hit-testing, clipping and the empty-callback
rule live.

**Seven copies, not five.** `CouncilBallotPopup` is the same widget again — parallel label and
payload vectors, index dispatch, the same `CacheEntryRects_` — and I edited it in this package
without noticing. Converted along with the rest.

**Rejected:** a CRTP or template base parameterised on payload type. It would put the layout and
hit-testing back in headers and re-instantiate them per payload, for no gain — the popups differ
only in what a row *says*, which is a `std::string`.

### [M] Long lists overflow with no clip or scroll

`CacheEntryRects_` walks the whole vector against a fixed line height and never clamps to
`m_layout.height`. Verified in all seven copies. With the shipped `line_height_ratio: 0.05` and
`header_line_offset: 2` that is ~18 rows; further entries paint past the chrome and fall outside
`Contains`, so they are neither visible nor clickable. Fixed once, in the shared component:
rows are bounded to the content area and the list scrolls with the arrow keys, with an overflow
indicator.

### [M] A row click can silently no-op

`ProductionSelectorPopup.cpp:100-106` requires both a non-null entry and a non-empty callback and
otherwise does nothing — the popup stays open with no feedback. `PopTypeSelectorPopup` does not
null-check at all. Since the shared component takes labels (never null) and an index, only the
callback can be absent, and an absent callback is a programmer error: the constructor throws.

### [H] Terraform key bindings are twenty-two compiled-in improvement ids

`TerraformInputController.h:31-54`. Verified: every id must stay byte-identical to
`config/improvements.json`, a mismatch surfaces only later inside `TryStartTerraform`, and a
modded hotkey needs a C++ edit. Moved to `config/ui/terraform_bindings.json` and validated
against `ImprovementRegistry` when the controller is constructed, so a bad id fails at startup
naming the key and the id.

### [H] `ComponentSlotDisplay` is compiled, styled, and never constructed

Verified: no `new`/`make_unique`/construction anywhere; `UnitDesignerView` builds `SlotColumnPanel`,
which reimplements the same fill/border/label/name paint and click callback. Deleted, with its
style block and its mention in the architecture diagram. **Rejected:** composing `SlotColumnPanel`
from `ComponentSlotDisplay` children — that is a redesign of a working panel to rescue an unused
class, and the guidelines are explicit about not keeping code with no current requirement.

### [M] Hardcoded ids and tables that drift from their sources

- `SupplyCrawlPopup.cpp:16-19` hardcodes the three crawl resources and their labels, duplicating
  `IsCrawlResource_` in `Unit.cpp:25-30`. The rule moves to one exported list the popup reads.
- `PopulationDisplay.cpp:110-121` derives a pop's glyph from the first letter of its type id, with
  hand-patched collisions (`Drone`/`Doctor` both `D` → `R`; `Talent`/`Technician` both `T` → `A`).
  A `display_glyph` on the pop type makes it data rather than a C++ special case. Note the
  **shipped glyphs are deliberately identical to today's rendering**, which means the remaining
  collisions stay (Empath/Engineer both `E`; Technician/Thinker/Transcend all `T`). The field
  makes them fixable in config; it does not by itself fix them.
- `SocialEngineeringDisplay.cpp:29-44` restates `SocialCategory_t` and `SocialRatingId_t` as
  hand-written arrays that must be edited when either enum grows. `magic_enum::enum_values`
  removes the second source of truth.
- The map renderer and `TileRenderer` hardcode improvement ids; `TileRenderer` was already moved
  to `ImprovementIds` by package 10, so only the map renderer remains.

### [M] `UnitStackPanel` silently drops units

`UnitStackPanel.cpp:49-57` stops laying out when the next slot would exceed the right edge, with
no scroll, wrap or cue. Verified: `SetUnits` accepts them, so units are unreachable rather than
merely unshown.

### [M] Duplicate identical style type pairs; elevation range in UI style

`GrowthDisplayStyle`/`ProductionDisplayStyle` and
`PopTypeSelectorPopupStyle`/`ProductionSelectorPopupStyle` are field-for-field identical with
identical JSON keys and identical shipped values, each with its own parser. And
`UiStyle.h:43-44` holds `minElevationMeters`/`maxElevationMeters`, duplicating the world-gen
preset range that actually governs terrain — a mod that widens elevation and does not also edit
`style.json` silently mis-colours the map. The UI copy is deleted and `TileRenderer` reads
`k_MinElevation`/`k_MaxElevation` from `Tile.h`.

**This removes the duplication but not the drift**, and the analysis originally overstated it:
those constants are Planet's absolute `SetElevation` clamp, not the *preset's* range. A preset of
±2000 still maps every land tile into the dark half of the colour ramp. Threading the active
preset's range into the renderer is the real fix and is not done here — recorded under Deferred.

## Review follow-ups

1. **[Not actually fixed] `UnitStackPanel::ScrollBy` had no callers.** I added a scroll method and
   an overflow cue, but nothing ever called the method — so the panel still dropped units, and now
   told the player about the ones it was dropping. Replaced with `ScrollSelectedIntoView_`, called
   from `SetUnits`, so the window follows the selection and the existing select-next-unit cycle
   reaches every unit. The unused public `ScrollBy` is gone.

2. **[Bug I introduced] The overflow indicator was drawn on top of the last visible row**, which
   also stayed clickable underneath it. `VisibleRowCount_` now reserves the bottom line when the
   list overflows. The tests did not catch this because the stub `Graphics` discards draw
   coordinates — a real gap: nothing in the suite asserts *where* anything is drawn.

3. **[Overstated claim] I said the unit designer's picker appearance was preserved. It is not.**
   Padding moved from a height basis to a width basis (the popup is never square, so this is a few
   pixels in both axes) and the first row moved from `0.13h` to `0.14h`. Also a lost degree of
   freedom: `title_height_multiplier` and `entry_height_ratio` were independent knobs and are now
   one `line_height_ratio`, so the header band only moves in whole row-heights. Small and
   acceptable for the consolidation, but it is a change, not a preservation.

4. `"Select " + rComponentType` rendered the raw config id — "Select chassis", and the same
   "Select ability" for both ability slots. Now the slot's `display_name`.

5. `border_width` used `.value(...)` with a default while every sibling field uses `.at(...)`,
   contradicting package 11's whole premise. Required now, and stated in `style.json`.

6. The terraform bindings path was hardcoded at a call site, the only config file in the tree not
   resolved through `GameDataPaths`. `GameDataContext` now carries the paths it was loaded from.

## Deferred from this package

- **[H] the `UiStyle` god-object/singleton** — see the scope decision above.
- **The elevation *domain*** (above): the UI no longer keeps its own copy, but the renderer uses
  Planet's absolute clamp rather than the active world-gen preset's range, so a narrow-range
  preset still colours flat. Threading the preset through `WorldDisplay` into `TileRenderer` is
  the fix.
- **Draw-position coverage.** The stub `Graphics` in `ListSelectorPopupTests` discards x/y, which
  is why the indicator/row collision was found by review rather than by test. A stub that records
  positions would let the layout be asserted.
- `ViewFactory` header narrowing (`[M]`) — mechanical include hygiene with no behavioural
  component; batched into package 16's sweep with the other `[L]` blocks.
