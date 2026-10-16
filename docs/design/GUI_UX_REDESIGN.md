# Host GUI/UX Redesign
**Status:** Open — phase 2 of 2

Phase 1 (§2-§9) implemented in full. §8 (cabinet node) by
`docs/archive/superpowers/plans/2026-09-10-cabinet-node.md`; §2-§7 by
`docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`. Phase 2 (§10-§18) implemented by
`docs/archive/superpowers/plans/2026-09-14-host-gui-phase2.md`, except §12's Pitch field, which is
drawn disabled until the track pitch-shift plan lands; its manual acceptance and the other
open items are in `docs/design/REMAINING_WORK.md`'s Host section.

Raw findings this spec is built from: `docs/archive/notes/2026-09-10-gui-ux-session-notes.md`.
Reference screenshots for Phase 1: `image.png`, `image1.png` (AmpliTube 5, the layout
model), `image4.png`, `image5.png`, `image6.png` (`sushidsp_host_gui.exe` states before
Phase 1), all at the repository root. Reference screenshots for Phase 2: `image1.png`
(signal-flow cable style), `image7.png`–`image10.png` (AmpliTube's series/parallel chain
templates), `image11.png` (the four chain-type icons). Phase 2's working reference is an
interactive HTML mockup, iterated live with the maintainer:
<https://claude.ai/code/artifact/86c82259-6c54-4fcf-be65-424903fd5c90>, every rule in
§10-§19 describes what that mockup does; read it alongside the mockup, not instead of it,
since the mockup is the source of truth for exact spacing, color and interaction feel and
this text is the source of truth for the data model and rules an ImGui port must preserve.

## §1 Goal

The current host GUI (`applications/host_gui`) has no coherent layout: the audio-device dialog opens
pinned to a corner, the device list hardcodes a device name instead of enumerating real
hardware, the transport bar sits at the top in an all-text style, and an unused file/folder
browser panel occupies screen space. This redesign brings the main window to an
AmpliTube-5-style layout, removes the hardcoded device entry, automates splitter/mixer
wiring, promotes the cabinet to a first-class rig component, and turns the mini-DAW into a
collapsible panel with a bounded track count.

## §2 Audio-device panel

**Implemented** by Task 1 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`.

- Opens **centered** in the main window, not pinned to the top-left corner.
- **Correction (2026-09-10):** the device list was assumed to hardcode a device name
    (`Roland Rubix`) based on what the panel showed during the walkthrough. Code
    inspection found no such hardcoding: `applications/host_gui/main.cpp:211-217` already calls
    `SushiDSP::AsioAudioDevice::enumerate_drivers()` and `applications/host_gui/ui/device_settings.cpp`
    already renders that list live; `Roland Rubix` in the screenshot was a real ASIO
    driver name enumerated from the machine it was captured on, not a literal in source.
    No `"Roland Rubix"` string exists anywhere under `applications/host_gui/`. This item is
    **dropped**: device enumeration is already correct and out of scope for this
    redesign.
- Centering is done: `DrawDeviceSettings` (`applications/host_gui/ui/device_settings.cpp:34`) now
  computes the display center and calls
  `ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f))` ahead of its
  `SetNextWindowSize`, so the panel opens centered each time it transitions from closed to
  open.

## §3 Main window layout (AmpliTube-5-style)

**Implemented** by Tasks 3 and 4 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`.

Reference: `image1.png`.

- **Top-left:** preset controls, save, load, and any other preset actions currently on the
  window (`Save`/`Load`/preset filename field in `image4.png`).
- **Center, split in two:**
  - **Left of center:** the settings panel for the currently selected rig node (amp,
    pedal, or cabinet controls).
  - **Right of center:** the gear browser (`applications/host_gui/ui/gear_browser.cpp`), moved from
    its prior position to sit right-of-center. Its internal presentation, a vertical list
    of full-width cards, one per row (`gear_browser.cpp:92`), is unchanged from before this
    plan; the grid layout described in an earlier draft of this section was never built and
    is tracked separately in `docs/design/REMAINING_WORK.md`.
- **Bottom:**
  - The transport/metronome bar (§5).
  - The rig design (chain) view, with the splitter-style picker at its far left edge,
    matching AmpliTube's left-side routing icons.
- This layout replaces the current arrangement in full; the present default screen
  (`image4.png`) is not incrementally adjusted; it is rebuilt to this structure.

## §4 Splitter / mixer automation

**Implemented** by Task 5 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`.

- The user never adds a splitter or mixer node by hand.
- The rig design view's splitter-style picker (§3, bottom) offers the available splitter
  styles. Selecting one:
  1. Inserts the matching splitter node (`sushidsp.splitter`, see
     `modules/nodes/routing/README.md`) at that point in the chain.
  2. Inserts the corresponding mixer node (`sushidsp.mixer`) where the parallel branches
     rejoin.
  3. Wires both ends automatically, the branches created by the split, and the rejoin
     into the mixer, with no manual connection step from the user.
- `sushidsp.splitter` and `sushidsp.mixer` (`modules/nodes/routing/`) already exist as ordinary
  rig nodes (per `docs/README.md`); this feature is GUI automation over node placement and
  wiring, not a new node type.

## §4a Chain types and what a change does to the gear

**Implemented** 2026-09-22, against AmpliTube 5's own behaviour as its user manual chapter 3
describes it; the research this is built from is in this session's report.

The rail offers three chain types, named after the manual's:

| Type | Paths | What it draws |
| --- | --- | --- |
| Single path | 1 | One straight run into the master bus |
| 2-ways splitter | 2 | Two paths between a splitter and a mixer |
| 3-ways splitter | 3 | Three paths between a splitter and a mixer |

AmpliTube's fourth type, **Parallel**, is stereo in and stereo out and is left out until the rig
renders in stereo; `docs/design/REMAINING_WORK.md` carries that work under the Host section.

**Path 0 is the main path.** It exists under every chain type and keeps its gear across every
change, which is the yellow path in AmpliTube's own colour code. The paths beside it are the
orange ones:

- A path that **opens** is filled with a copy of the main path, each node's parameter values
  included, so a split starts from what was already being heard rather than from silence.
- A path that **closes** takes its gear with it; the slots it held are destroyed.
- The master bus opens with the second path and closes with it. One path is the whole chain, so
  it carries no master bus: a stage drawn nowhere that still routed gear would be audible with
  nothing on screen to account for it.

`RigTopology` holds identities, not nodes, so it cannot copy gear itself: `set_chain_type`
reports which paths opened, and `TopologyApplier::switch_chain_type` does the duplication
through the rig's own snapshot and restore. This is why the two are one call at the view's
site, a caller that switched the type without filling the paths would leave a split running
silent on every branch but the first, which is what the rail did before this section existed.

## §5 Transport / metronome bar

**Implemented** by Tasks 2 and 4 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`.

Reference: `image6.png` (current), `image1.png`'s bottom bar (target style).

- Moves from the top of the window (current) to the bottom, beside/above the rig design
  view per §3.
- Redesigned to prefer icon/symbol controls over text buttons wherever a clear symbol
  exists (record dot, play/stop/loop transport glyphs, a metronome icon for Click), for a
  visually clean, uncluttered bar. Controls with no natural symbol (BPM number entry, time
  signature, tape-speed multiplier) keep numeric/text fields.
- No functional change to transport behavior in this pass, this is a presentation and
  placement change only.

## §6 Mini-DAW panel

**Implemented** by Task 6 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`.

- Functionally unchanged from the current REC/Play/Stop/Loop/lane-view behavior
  (`image4.png`).
- Becomes a **collapsible panel**, hidden by default, toggled by one clearly-labeled
  button. Its internal layout is not redesigned in this pass.
- **Correction (2026-09-10):** track count was assumed to need a user-facing
    add/remove control, capped at 8. Code inspection found the track count is already a
    fixed compile-time constant of exactly 8 throughout the host
    (`SushiDSP::TrackPlayer::K_MAX_TRACKS = 8` in
    `modules/nodes/capture/include/SushiDSP/nodes/capture/track_player.hpp:51`, mirrored by
    `SessionTimeline::tracks_` as `std::array<TrackInfo, TrackPlayer::K_MAX_TRACKS>` in
    `applications/host_gui/session_timeline.hpp`), with no add/remove-track API anywhere in
    `RigController` or `ArrangementView`, `ArrangementView` always draws exactly eight
    lane headers. Fixed-8 already matches the intended cap; **no add/remove-track
    feature is added in this pass.**

## §7 Remove the file/folder browser panel

**Implemented** by Task 3 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md`.

- The panel shown in `image5.png` ("Drag a take or a song onto a lane, or c[lick...]" /
  "(no folder)" file browser, currently under the gear list in `image4.png`) is **removed
  entirely**, not hidden, not collapsed. No replacement panel takes its place; the space
  it occupied is reclaimed by the redesigned layout in §3.

## §8 Cabinet as a first-class rig component

- A new `sushidsp.cabinet` entry appears in the gear browser (§3) alongside amps and
  pedals, draggable into the rig chain the same way.
- It loads an impulse response (IR) and convolves it into the signal path.
- **Former mechanism:** `modules/foundation/core/include/SushiDSP/foundation/core/cabinet_host.hpp` defined the cabinet
  contract (`set_cabinet` / `has_cabinet` / `reclaim_cabinets`) that amp node classes
  (`modules/nodes/jcm800`, `modules/nodes/peavey5150`, `modules/nodes/diezel_vh4`, `modules/nodes/mesa_mark2c`)
  implemented directly. There was no standalone cabinet `AudioNode`; IR loading
  (`modules/foundation/io/include/SushiDSP/foundation/io/ir.hpp`, `ir_loader.hpp`) and convolution
  (`modules/foundation/math/include/SushiDSP/foundation/math/partitioned_convolution.hpp`,
  `modules/foundation/math/include/SushiDSP/foundation/math/streaming_convolution.hpp`) were owned by whichever amp hosted them.
- **Decision (owner, 2026-09-10), implemented:** `sushidsp.cabinet` is now the only cabinet
  path, delivered by `docs/archive/superpowers/plans/2026-09-10-cabinet-node.md` (all 8 tasks
  complete). The amp node classes (`modules/nodes/jcm800`, `modules/nodes/peavey5150`,
  `modules/nodes/diezel_vh4`, `modules/nodes/mesa_mark2c`) dropped their `CabinetHost` implementation
  and the built-in convolution it drove; `CabinetHost` is now the interface
  `sushidsp.cabinet` alone implements. A rig that wants a cabinet places the
  `sushidsp.cabinet` node after its amp explicitly; no amp has a cabinet baked in and there
  is no dual-path (no scenario where both an amp's built-in cabinet and a standalone
  cabinet node could convolve the same signal). This changed each amp's default signal
  path, an amp rig that relied on the built-in cabinet now needs an explicit
  `sushidsp.cabinet` node added to sound the same, which was an intentional, one-time break
  tracked as part of this redesign, not a regression to avoid.

## §9 Out of scope

- No new audio backend or device-enumeration API is introduced; §2 only replaces the
  hardcoded list with an existing backend's enumeration call.
- No functional change to transport/metronome or mini-DAW behavior, both passes are
  presentation/placement/lifecycle (collapsible, track cap) only.
- No change to `sushidsp.splitter` / `sushidsp.mixer` node behavior, §4 automates their
  placement in the GUI, not their DSP.

---

# Phase 2, rig chain editor & workspace redesign

## §10 Goal

Phase 1 gave the host a coherent AmpliTube-style shell but left three things unresolved:
the transport bar and the rig chain were separate panels instead of one working surface,
the splitter/mixer automation from §4 rendered as full-size nodes indistinguishable from
an amp or a pedal, and no panel could be resized, every split between areas was a fixed
ratio. Phase 2 replaces the §3/§4/§5/§6 layout with a single resizable workspace built
around how a signal actually branches and rejoins, and turns the splitter picker from "insert
a routing node at the cursor" into "pick the whole rig's topology," which is what the
AmpliTube reference (`image7.png`-`image11.png`) actually shows: the four left-rail icons
choose between a straight run and two- or three-way parallel racks, they are not something
you drop mid-chain.

## §11 Workspace layout

The window is three fixed regions stacked vertically, each independently resizable against
its neighbors by dragging the divider between them (§18):

- **Top bar**: unchanged from §3's top-left preset controls, plus the audio-device
  connection pill and a settings button, now moved to a right-hand group; **"SushiDSP"**
  is centered in the bar as a fixed brand mark, independent of how wide the left/right
  groups are (a three-column layout: left group, centered brand, right group, the brand
  never shifts off-center when the side groups change width).
- **Workspace**: the single resizable area described in §13-§17. It swaps between two
  mutually exclusive views, never both at once: the rig editor (node settings + gear
  browser + rig chain) and the mini-DAW (§15).
- **Bottom transport bar**: fixed at the very bottom of the window, always visible
  regardless of which workspace view is showing (§12).

This replaces §3's "top-left preset / center split / bottom transport+chain" arrangement:
the transport bar sits apart from the rig chain, in its own permanent strip below
everything, and the rig chain now lives inside the same resizable block as the node
settings and gear browser.

## §12 Bottom transport bar

Holds exactly what §5 specified (record/play/stop/loop, click, BPM, time signature, tap
tempo, playback speed) plus two additions:

- **Pitch**: a semitone offset field (`0 st`), beside Speed, same numeric-field treatment
  as BPM/time-signature (no natural icon, so it stays a text field per §5's rule).
- **Live input/output level meters**: two small horizontal meters at the bar's right edge,
  each with a numeric dB readout beside it, refreshing continuously while the bar is
  visible. This is the first place in the host GUI a live audio measurement is shown
  outside a node's own controls; the ImGui port reads the same peak/RMS values
  `applications/host_gui` already computes for its own level display, if one exists, or is
  scoped as its own small addition if not, the mockup drives its meters with synthetic
  sine-plus-noise data and that is not a behavior to preserve, only the visual.
- A **Mini-DAW** toggle button, replacing §6's separate collapsible panel, see §15.

The bar's height is user-resizable (§18) but never disappears; it is the one region always
present regardless of which workspace view is active.

## §13 Node settings + gear browser + rig chain merge

§3's "settings panel left of center, gear browser right of center" split is gone. The rig
editor view is now three parts in an L-shape:

- **Right:** the gear browser (§14), full pane height, resizable width.
- **Left, split horizontally:** node settings on top (resizable height), the rig chain
  view on the bottom (fills what's left).

Node settings shows the currently selected chain element's controls (knobs, matching the
element's type, amp/pedal/cabinet) or an empty-state prompt when nothing is selected.
Selection comes from clicking any node in the rig chain below, in a branch (§16), or in the
pre/post stage around a split (§17); dragging a gear card onto the chain also selects the
node it creates. This is unchanged from §3's node-settings behavior, only its position
(now top-left instead of left-of-center) and the fact that gear browser and chain now share
the same column group have changed.

## §14 Gear browser: list or grid, user's choice

Closes the open item in `docs/design/REMAINING_WORK.md`'s Host section ("gear-browser grid").
The browser keeps §3's search field and type tabs (All/Amps/Pedals/Cabs) and adds a
two-button toggle in its header: **list** (§3's original vertical, full-width card per row)
and **grid** (a wrapping grid of compact icon tiles, name only, no subtitle). Both modes
show the same category grouping and the same drag source, only the card layout changes.
The choice is a pure view preference with no effect on the rig; default is list, matching
what §3 already shipped.

## §15 Mini-DAW as a full workspace swap, not a drawer

§6 made the mini-DAW a collapsible panel stacked below the rig chain. Phase 2 replaces
that: the **Mini-DAW** button in the bottom bar (§12) swaps the entire workspace's content
,  node settings, gear browser and rig chain disappear together, for the arrangement view
(§6's unchanged REC/Play/Stop/Loop/lane behavior, fixed 8 tracks, per-track mute/solo).
The same button, now reading "Back to Rig," swaps back. Only one of the two views is ever
mounted; there is no partial/collapsed state as there was in §6. The arrangement view's own
internal layout is one further resizable split: a fixed-position lane-header column against
the scrolling grid, its width draggable independently of every other divider in the window.

## §16 Chain type: a topology choice, not an inserted node

This is the behavioral core of Phase 2 and the part most likely to be misread from the
AmpliTube screenshots. The splitter picker (the four icons at the rig chain's left edge,
`image11.png`) does **not** insert a splitter/mixer pair at wherever the cursor or selection
is, the way an earlier draft of this section modeled it. It selects **one of four topologies
for the entire chain**, matching `image7.png`-`image10.png`:

1. **Series** (`image7.png`), a single straight run, exactly what §3/§4 already built:
   IN → node → node → … → OUT, gear dragged from the browser and reordered freely.
2. **2-Way Split** (`image8.png`), IN branches into two fully independent parallel racks
   that rejoin before OUT.
3. **3-Way Split** (`image9.png`), the same, three racks.
4. **2-Way Split (Alt)** (`image10.png`), a second two-way icon/label for a topology the
   maintainer wants available as a distinct preset from #2 even though today's mockup gives
   it identical behavior to #2; do not collapse the two into one button, and do not assume
   they must stay behaviorally identical if the maintainer later asks for them to diverge
   (e.g. a DI/pre-stage present in one and not the other, visible in `image8.png` vs
   `image10.png`).

Choosing a non-series type is a **destructive switch**: each branch starts empty, and the
maintainer confirmed (in the session that produced this doc) that switching between the two
2-way variants or between 2-way and 3-way is allowed to discard whatever gear was in the
branches, the pre/post stages (§17) are the one thing that survives every chain-type
switch. Switching back to Series does not discard the series chain that was active before
switching away from it: the host should keep `chain` (series) and `branches` (parallel)
as separate state, so toggling to a split type and back to Series round-trips the original
series content. This matches the mockup's behavior exactly (`chainType`, `chain`, and
`branches` are three separate pieces of state; only `branches` is reset on a type change).

The splitter and mixer are **rendered as small fixed icon glyphs at the chain's left and
right ends of the parallel section, never as a draggable/removable chain element**. They
carry no settings panel, no remove button, no drag handle. Each parallel branch is a full,
independently editable sub-chain (drag gear in, reorder, remove, identical rules to the
Series chain, just scoped to that branch's own array instead of the whole rig), rendered as
a row of normal-sized node boxes, one row per branch, numbered 1..N. The signal path is
drawn as a routed cable, a rounded-corner line from the splitter glyph to each branch row
and from each branch row to the mixer glyph, matching the AmpliTube reference's curved
orange/teal wire style, not the small dashed-slot abstraction an earlier draft of this
section used.

## §17 Pre/post stages around a split

When a non-series chain type is active, the region between IN and the splitter glyph, and
the region between the mixer glyph and OUT, are themselves small editable chains (gear can
be dragged in, reordered, removed), not static gaps. This is where a DI box, tuner, or
input gain stage goes before the split, and where a final EQ or output stage goes after the
mixer, matching what `image8.png`'s DI box and `image1.png`'s post-amp EQ/output icons show.
These two stages are shared across all three non-series chain types (switching between
2-Way, 3-Way and 2-Way-Alt keeps them intact, per §16) and are simply not rendered while
Series is active, Series has no split, so there is nothing before or after one.

## §18 Everything is resizable

Every fixed split in Phase 1 becomes a user-draggable divider in Phase 2:

- Gear browser width against the node-settings/rig-chain column.
- Node settings height against the rig chain height, within that column.
- The bottom transport bar's height against the workspace above it.
- The mini-DAW's lane-header column width against its scrolling grid.

There is no "reset to default" affordance specified; a divider is dragged freely between a
sane minimum and the remaining available space, and the chosen sizes are not required to
persist across a restart unless a later phase adds preset/window-state persistence, Phase 2
is layout and interaction only.

## §19 Out of scope

- No change to what a node's settings panel shows or how its knobs behave, §13 only moves
  the panel, exactly as §9 said for Phase 1's settings-panel move.
- No new gear/node types. The gear browser's catalog (amps, pedals, cabinets) is unchanged;
  §14 only adds a second way to browse the same catalog.
- No change to `sushidsp.splitter` / `sushidsp.mixer` DSP behavior, same boundary §9 drew
  for Phase 1, §16 changes how the GUI presents and edits a parallel topology, not what
  `SplitterNode`/`MixerNode` compute.
- No audio-engine wiring for the live meters (§12) or for saving/loading a rig that uses a
  non-series chain type, Phase 2 is the GUI and its data model; persisting a parallel
  topology through `modules/session/rig_preset/` is separate work, not committed to by this
  document.
- No window-state persistence (§18's divider positions, chosen gear-browser view mode,
  or last-used chain type) across a restart.
