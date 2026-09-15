# Host GUI Layout Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild `apps/host_gui`'s main window to an AmpliTube-5-style layout: a centered
audio-device panel, preset controls top-left, a center split between node settings and the
gear browser, a bottom transport bar with icon controls and the rig design view (splitter
style at its far left, driving automatic splitter/mixer wiring), a collapsible mini-DAW, and
removal of the unused takes/file-browser panel.

**Architecture:** `apps/host_gui/main.cpp` owns every top-level region's screen position
today (`ImGui::SetNextWindowPos`/`SetNextWindowSize` per region, `main.cpp:536-670` and
beyond) — this is the single file most tasks below touch, so layout tasks in this plan run
**serially**, not in parallel, to avoid two tasks racing on the same region-layout block.
Tasks confined to one panel's own file (`DeviceSettings.cpp`, `TransportBar.cpp`) are
independent of the layout block and of each other, and run in their own wave.

**Tech Stack:** Dear ImGui (immediate mode) + GLFW/OpenGL3, C++17. No ImGui-rendering test
harness exists in this codebase today (confirmed by code survey: `grep -rl
"host_gui\|ImGui" tests/` matches only headless logic tests —
`test_knob_math.cpp`, `test_rig_controller.cpp`, `test_rig_preset.cpp` — none exercise
rendering). This plan follows that existing convention: layout/rendering tasks are verified
by building and visually inspecting the running app (`sd build`, then launch and screenshot,
per the `run` skill), not by a new test harness this plan does not introduce speculatively.
Logic that has no rendering component (e.g. the splitter-style picker's call into
`RigController::add_branch`) is still unit-tested, since `RigController` already has ctest
coverage.

**Spec:** `docs/design/gui_ux_redesign.md` §2-§7.

## Global Constraints

- C++17, Allman braces, `PascalCase` types, `snake_case` functions/variables, no
  abbreviations, Apache 2.0 header box on any new file, Doxygen on public functions.
- `apps/host_gui` is not on the real-time audio thread — normal C++ exception/allocation
  rules apply here, unlike `modules/`. Do not import RT-safety constraints into GUI code.
- Build and test only through `sd build` / `sd test`, never `cmake`/`ctest` directly; never
  kill the running GUI process without asking the user first (per session convention).
- Every subagent prompt for this plan opens with the four lines the user's global
  CLAUDE.md requires: SOLID-brick discipline, the `humanizer` skill on any prose written,
  never invoke `sd`/`cmake`/`ninja`/`ctest` and never edit build configuration files, and
  report what was done and what wasn't with every verification's output pasted. For this
  plan specifically, "verification" for a layout task means a screenshot of the built,
  running app plus a one-line description of what it shows — paste the screenshot path in
  the report, not just a claim.

---

## Wave 1 — panel-local changes (independent files, run in parallel)

### Task 1: Center the audio-device panel

**Files:**
- Modify: `apps/host_gui/ui/DeviceSettings.cpp`

**Interfaces:**
- No signature change to `DrawDeviceSettings` — internal only.

- [ ] **Step 1: Add the centering call**

In `DrawDeviceSettings` (`DeviceSettings.cpp:34`), before the existing
`ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);` line, add:

```cpp
const ImGuiViewport* viewport = ImGui::GetMainViewport();
const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                     viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
```

`ImGuiCond_Appearing` matches the existing size call, so the panel centers each time it
transitions from closed to open (its `open` flag flips false→true), and a user who then
drags it keeps their placement across frames — it does not re-center every frame.

- [ ] **Step 2: Build and verify visually**

Hand the user: `sd build`. Once built, launch `build/apps/host_gui/sushidsp_host_gui.exe`
(ask before launching if one may already be running), open the audio-device panel, and
screenshot it. Expected: the panel opens centered in the window, not pinned to the top-left
corner. Paste the screenshot path in the report.

- [ ] **Step 3: Commit**

```bash
git add apps/host_gui/ui/DeviceSettings.cpp
git commit -m "Center the audio-device panel on open

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 2: Redesign the transport/metronome bar with icons

**Files:**
- Modify: `apps/host_gui/ui/TransportBar.cpp`

**Interfaces:**
- No change to `void DrawTransportBar(RigController& rig, std::string& transport_error);`
  — internal rendering only, so this task is independent of where `main.cpp` places the
  `##transport` region (Task 4).

- [ ] **Step 1: Replace text buttons with icon/symbol glyphs**

Dear ImGui has no built-in icon font in this project (no `IconsFontAwesome`-style include
found in the survey) — introducing one is out of scope for this pass (it would need a new
font asset and a loader change, neither requested). Use ImGui's built-in Unicode glyph
support instead, which the default font (`ImGui::GetIO().Fonts`) already renders for the
common transport symbols:
- Record: `"\xE2\x97\x8F"` (●, U+25CF) in red (`ImGui::PushStyleColor(ImGuiCol_Text,
  to_imvec4(rgba(220, 72, 60)))` while recording) instead of the `"REC"`/`"STOP REC"` text.
- Play: `"\xE2\x96\xB6"` (▶, U+25B6) instead of `"Play"`.
- Stop: `"\xE2\x96\xA0"` (■, U+25A0) instead of `"Stop"`.
- Rewind-to-start: `"\xE2\x8F\xAE"` (⏮, U+23EE) instead of `"|<"`.
- Metronome/click toggle: `"\xE2\x99\xAA"` (♪, U+266A) instead of the `"Click"` label,
  toggled the same way the existing `Click` checkbox is today.
Keep numeric/text fields as text — BPM (`DragFloat`), time signature (`4`/`4`), tape speed
(`1.00x`), TAP — none of these has a universally clear glyph, matching spec §5's carve-out.
Wrap each icon button in `ImGui::PushID`/`PopID` where the current code already does (the
button identity, not its label, drives ImGui's ID stack here since labels change per state
— check `TransportBar.cpp`'s existing REC/STOP REC toggle at line 123 for the pattern
already in use, and keep using it so the widget doesn't lose focus/hover state on toggle).

- [ ] **Step 2: Build and verify visually**

Hand the user: `sd build`, launch the app, screenshot the transport bar. Expected: record,
play, stop, rewind and click controls show as symbols rather than text buttons; BPM, time
signature, tape speed and TAP remain as before. Paste the screenshot path.

- [ ] **Step 3: Commit**

```bash
git add apps/host_gui/ui/TransportBar.cpp
git commit -m "Use icon glyphs for the transport bar's action buttons

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Wave 2 — main window layout rebuild (serial: all touch `main.cpp`'s region block)

These three tasks run **in this order**, each building on the previous task's edit to the
same `main.cpp` region-layout block (`main.cpp:536-670+`). Do not parallelize them.

### Task 3: Remove the takes/file-browser panel

**Files:**
- Modify: `apps/host_gui/main.cpp`

**Interfaces:**
- No change to `ui::FileBrowser`'s own header/API — `apps/host_gui/ui/StagePanel.cpp:152`
  still calls the single-extension `FileBrowser()` overload for the cabinet IR browser, so
  `FileBrowser.cpp`/`.hpp`/`DirectoryListing.cpp`/`.hpp` stay in
  `apps/host_gui/CMakeLists.txt`'s source list unchanged — only the **call site** in
  `main.cpp` that draws the `##takes` region is removed.

- [ ] **Step 1: Delete the `##takes` region**

In `main.cpp`, delete the whole block from `ImGui::SetNextWindowPos(ImVec2(width -
browser_width, body_top + gear_height));` (`main.cpp:645`) through its matching
`ImGui::End();` (after the drag-and-drop import logic that follows the `FileBrowser(...)`
call, `main.cpp:659-`, continuing to that region's closing `End()` — read the file from
line 645 to find the exact closing brace/`End()` before deleting, since the block's tail
was outside the fetched 530-670 excerpt).

Also delete the now-unused `#include "ui/FileBrowser.hpp"` at `main.cpp:55` **only if**
nothing else in `main.cpp` calls into `ui::FileBrowser`/`ui::is_take_file` — grep
`main.cpp` for `FileBrowser|is_take_file` first; if another call site remains, keep the
include.

- [ ] **Step 2: Reclaim the space in the gear-browser column**

Before this change, `gear_height` (`main.cpp:557`) sized the `##gear` region to leave room
below it for `##takes`. With `##takes` gone, the gear browser column should take the full
`body_height` in that column (`browser_width` wide) rather than stopping at `gear_height`.
Change the `##gear` region's size call (`main.cpp:640`,
`ImGui::SetNextWindowSize(ImVec2(browser_width, gear_height));`) to
`ImGui::SetNextWindowSize(ImVec2(browser_width, body_height));`, and delete the
now-unused `gear_height` local (`main.cpp:557`) if nothing else reads it — grep first.

- [ ] **Step 3: Build and verify visually**

Hand the user: `sd build`, launch, screenshot the right-hand column. Expected: no "Drag a
take or a song onto a lane" panel anywhere; the gear browser fills the space it used to
share with that panel; dragging a cabinet IR into an amp via `StagePanel`'s own file browser
(unrelated panel) still works. Paste the screenshot path.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/main.cpp
git commit -m "Remove the unused takes/file-browser panel

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 4: Rebuild the main window into the AmpliTube-5-style region layout

**Files:**
- Modify: `apps/host_gui/main.cpp`

**Interfaces:**
- Consumes: `ui::DrawTopBar`, `ui::DrawTransportBar`, `ui::DrawGearBrowser`,
  `ui::DrawStagePanel`/`ui::DrawRoomPanel`, `ui::DrawChainStrip` — all pre-existing
  draw functions, called with their existing signatures; only their window region
  (position/size) changes, not which function draws what.

- [ ] **Step 1: Re-derive the region geometry**

Replace the region-geometry block (`main.cpp:541-557`, from `int display_w = 0;` through
`const float gear_height = ...`) with geometry matching spec §3:

```cpp
int display_w = 0;
int display_h = 0;
glfwGetFramebufferSize(window, &display_w, &display_h);
const float width = static_cast<float>(display_w);
const float height = static_cast<float>(display_h);

const float preset_bar_height = 40.0f;   // top-left preset controls (Save/Load/name)
const float transport_height = 76.0f;    // bottom transport/metronome bar
const float rig_design_height = 160.0f;  // bottom rig-design (chain) view, splitter picker at its left
const float bottom_height = transport_height + rig_design_height;
const float center_top = preset_bar_height;
const float center_height = std::max(height - preset_bar_height - bottom_height, 240.0f);
const float settings_width = width * 0.55f;   // left-of-center: selected node's settings
const float gear_width = width - settings_width; // right-of-center: gear browser
```

- [ ] **Step 2: Reposition each region to match**

- `##top` (preset controls, `ui::DrawTopBar`): `SetNextWindowPos(0, 0)`,
  `SetNextWindowSize(width, preset_bar_height)` — stays top-left/full-width, only its
  height changes from the old `top_height` (48) to `preset_bar_height` (40); adjust if
  `ui::DrawTopBar`'s content needs more room (verify visually in Step 4, revise the
  constant if controls clip).
- `##settings` (was `##stage`, `ui::DrawStagePanel`/`ui::DrawRoomPanel`):
  `SetNextWindowPos(0, center_top)`, `SetNextWindowSize(settings_width, center_height)`.
- `##gear` (`ui::DrawGearBrowser`): `SetNextWindowPos(settings_width, center_top)`,
  `SetNextWindowSize(gear_width, center_height)`.
- `##transport` (`ui::DrawTransportBar`): `SetNextWindowPos(0, height - bottom_height)`,
  `SetNextWindowSize(width, transport_height)` — moved from just under `##top` to the
  bottom, per spec §5.
- `##chain` (`ui::DrawChainStrip`, the rig design/chain view): `SetNextWindowPos(0, height
  - rig_design_height)`, `SetNextWindowSize(width, rig_design_height)` — below the
  transport bar, matching AmpliTube's bottom-of-window chain strip. The splitter-style
  picker (Task 5) renders at this region's left edge; `DrawChainStrip`'s own call signature
  is unchanged in this task — Task 5 extends it separately.

- [ ] **Step 3: Check the arrangement/meter regions below `##chain` still fit**

`main.cpp` positions the arrangement view and the level meter bar below the body region
today (past line 670, not fully in this plan's citations — read that part of the file
directly before editing). With the rig design view now anchored to the window's bottom,
these regions become the collapsible mini-DAW panel (Task 6) rather than always-visible
fixed regions — do not reposition them permanently in this task; Task 6 owns making them
collapsible. For this task, it is enough that the window still compiles and every region
above is correctly placed; leave the arrangement/meter regions' current position as a
placeholder to be revisited by Task 6, and note in this task's report which lines you found
them at.

- [ ] **Step 4: Build and verify visually**

Hand the user: `sd build`, launch, screenshot the full window. Expected: preset controls
top-left, node settings left-of-center, gear browser right-of-center, transport bar and rig
design view at the bottom, matching `docs/design/gui_ux_redesign.md` §3's description and
the `image1.png` reference. Paste the screenshot path and compare it point-by-point against
§3 in the report.

- [ ] **Step 5: Commit**

```bash
git add apps/host_gui/main.cpp
git commit -m "Rebuild the main window into the AmpliTube-5-style layout

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

### Task 5: Splitter-style picker with automatic splitter/mixer wiring

**Files:**
- Create: `apps/host_gui/ui/SplitterPicker.hpp`
- Create: `apps/host_gui/ui/SplitterPicker.cpp`
- Modify: `apps/host_gui/main.cpp` — call the new picker at the left edge of the `##chain`
  region established in Task 4.
- Modify: `apps/host_gui/CMakeLists.txt` — add the two new source files.

**Interfaces:**
- Produces: `void ui::DrawSplitterPicker(RigController& rig, RigController::SlotId
  target_splitter_or_chain_end);` — draws the splitter-style icons at the chain view's left
  edge; on click, calls `RigController::add_node("sushidsp.splitter", false)` then
  `RigController::add_branch(splitter_slot, descriptor_id)` (both pre-existing,
  `RigController.hpp:88,155`) to insert the splitter and let `add_branch` create/reuse the
  matching mixer automatically, exactly as `add_branch`'s existing doc comment describes.
- Consumes: `RigController::add_node`, `RigController::add_branch`,
  `RigController::connect` — all pre-existing, unchanged.

- [ ] **Step 1: Confirm there is exactly one splitter "style" today**

The code survey found one splitter shape (`SplitterNode`, `type_id() ==
"sushidsp.splitter"`) — there is no existing notion of multiple splitter *styles* (e.g. Y-
split vs. A/B) in `modules/routing/`. Before writing the picker, re-read
`modules/routing/include/SushiDSP/routing/SplitterNode.hpp` to confirm this is still true.
If it is, the picker offers a single "Split" icon/button rather than a style list — spec §4
describes automation behavior (never manual wiring), not multiple distinct splitter shapes,
so a single style satisfies it. If a second splitter shape has since been added, stop and
ask the user how to distinguish them in the picker before proceeding — do not guess a
visual scheme for a shape this plan was not written against.

- [ ] **Step 2: Write `SplitterPicker.hpp`**

```cpp
#pragma once

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the splitter-style picker at the rig design view's left edge. Clicking
    /// an entry inserts a splitter after `chain_end` and automatically adds/reuses the
    /// matching mixer, with no manual node placement from the user.
    /// @param rig The rig to edit.
    /// @param chain_end The slot to split after — typically the last slot in chain order.
    void DrawSplitterPicker(RigController& rig, RigController::SlotId chain_end);
}
```

- [ ] **Step 3: Write `SplitterPicker.cpp`**

```cpp
#include "ui/SplitterPicker.hpp"

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    void DrawSplitterPicker(RigController& rig, RigController::SlotId chain_end)
    {
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Split");
        if (ImGui::Button("Y##splitter_style_y"))
        {
            const RigController::SlotId splitter = rig.add_node("sushidsp.splitter", false);
            rig.connect(chain_end, splitter, false);
            rig.add_branch(splitter, "sushidsp.jcm800");
        }
        ImGui::EndGroup();
    }
}
```

The branch's default `descriptor_id` (`"sushidsp.jcm800"` above) is a placeholder choice —
ask the user during implementation what the second branch should default to (an amp, a
copy of the first branch's chain, or an empty slot the user fills from the gear browser)
before finalizing this step; this is a product decision this plan was not given an answer
to, not a detail to pick arbitrarily.

- [ ] **Step 4: Wire it into `main.cpp` and `CMakeLists.txt`**

Add `#include "ui/SplitterPicker.hpp"` to `main.cpp`'s include block (Task 4's `##chain`
region), and call `ui::DrawSplitterPicker(rig, chain_end_slot)` at the start of the
`##chain` region's body, before `ui::DrawChainStrip(...)`, so it renders at the region's
left edge. Add `ui/SplitterPicker.cpp` to `apps/host_gui/CMakeLists.txt`'s source list next
to `ui/ChainStrip.cpp`.

- [ ] **Step 5: Build and verify visually, plus a headless RigController check**

Hand the user: `sd build`, launch, click the splitter picker, screenshot the resulting
chain. Expected: a splitter and a mixer both appear in the chain strip after one click, with
no manual node-add or connect step. Separately, `RigController::add_branch`'s existing test
coverage (`tests/functional/unit/test_rig_controller.cpp`,
`RigControllerAddBranchBuildsTheParallelShapeTheGuiOffers` and its siblings) already proves
the underlying wiring is correct — this task adds no new `RigController` test since it adds
no new `RigController` behavior, only a GUI call site. Paste the screenshot path.

- [ ] **Step 6: Commit**

```bash
git add apps/host_gui/ui/SplitterPicker.hpp apps/host_gui/ui/SplitterPicker.cpp apps/host_gui/main.cpp apps/host_gui/CMakeLists.txt
git commit -m "Add the splitter-style picker with automatic splitter/mixer wiring

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Wave 3 — mini-DAW collapsibility (depends on Task 4's region layout)

### Task 6: Make the mini-DAW panel collapsible, hidden by default

**Files:**
- Modify: `apps/host_gui/main.cpp`

**Interfaces:**
- No change to `ui::DrawArrangementView`'s signature — only whether/when `main.cpp` calls
  it.

- [ ] **Step 1: Add a visibility flag and a toggle button**

In `main.cpp`'s per-frame state (alongside `take_directory`/`transport_error` at the top of
the render loop), add `static bool mini_daw_open = false;`. In the `##top` (preset bar)
region from Task 4, add one clearly-labeled toggle button, e.g. `if
(ImGui::Button(mini_daw_open ? "Hide DAW" : "Show DAW")) { mini_daw_open =
!mini_daw_open; }`.

- [ ] **Step 2: Gate the arrangement/meter regions on the flag**

Wrap the arrangement-view region(s) found in Task 4 Step 3 (the code past `main.cpp:670`
that calls `ui::DrawArrangementView` and the meter bar) in `if (mini_daw_open) { ... }`.
When closed, the window keeps its current full height (no gap left where the panel would
have been) — when open, grow the window's effective content area to fit it below `##chain`,
or overlay it as its own region sized against `height` the same way `##chain` is in Task 4;
pick whichever keeps `##chain`'s position fixed, since the rig design view must stay
reachable regardless of the mini-DAW's visibility. Confirm the choice visually in Step 3
before committing to a layout constant.

- [ ] **Step 3: Build and verify visually**

Hand the user: `sd build`, launch. Expected on first launch: no arrangement/timeline view
visible, only the "Show DAW" button. Click it: the mini-DAW panel (REC/Play/Stop/Loop, lane
view) appears; click "Hide DAW": it disappears again. Screenshot both states.

- [ ] **Step 4: Commit**

```bash
git add apps/host_gui/main.cpp
git commit -m "Make the mini-DAW panel a collapsible, hidden-by-default panel

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Documentation

### Task 7: Update the manual and changelog

**Files:**
- Modify: `docs/reference/CHANGELOG.md` — one line: `- 2026-09-10 — Rebuilt the host GUI's
  main window into an AmpliTube-5-style layout, centered the audio-device panel, added the
  splitter picker and a collapsible mini-DAW, and removed the unused takes panel
  (\`apps/host_gui\`).`
- Modify: `docs/design/REMAINING_WORK.md` — remove the GUI/UX redesign entry from the Host
  section once both this plan and `2026-09-10-cabinet-node.md` are fully executed and
  verified; until then, leave it and instead check off its items informally in the entry
  text (or split it into "done"/"open" sub-bullets) so the backlog stays honest about
  partial progress.

- [ ] **Step 1: Write the changelog line, update the backlog entry**

- [ ] **Step 2: Commit**

```bash
git add docs/reference/CHANGELOG.md docs/design/REMAINING_WORK.md
git commit -m "Document the host GUI layout redesign

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_0194iyViCa7vt5xWm1YKeveX"
```

---

## Self-Review

1. **Spec coverage:** §2 (centering) → Task 1. §3 (layout) → Task 4, with Task 3 clearing
   the takes panel out of the way first. §4 (splitter/mixer automation) → Task 5. §5
   (transport bar, bottom + icons) → Task 2 (icons) + Task 4 (position). §6 (collapsible
   mini-DAW, fixed-8 tracks per the corrected spec) → Task 6; no track add/remove task
   exists, matching the spec's correction. §7 (remove file browser) → Task 3.
2. **Placeholder scan:** two deliberate open decisions are flagged, not hidden — Task 5
   Step 1 (single vs. multiple splitter styles) and Step 3 (the second branch's default
   descriptor) — both instruct the implementer to stop and ask rather than guess, per this
   project's "ask before deciding... never pick arbitrarily" rule; these are not silent
   placeholders because each names exactly what question to ask and why.
3. **Type consistency:** `ui::DrawSplitterPicker`'s `RigController&`/`SlotId` parameters
   match `RigController`'s existing public API throughout (Task 5); no new type is
   introduced that a later task also names differently.
