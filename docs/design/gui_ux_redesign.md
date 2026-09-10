# GUI/UX redesign — host app

**Status:** Design — open phase, nothing implemented yet.

Raw findings this spec is built from: `docs/agent/notes/2026-09-10-gui-ux-session-notes.md`.
Reference screenshots: `image.png`, `image1.png` (AmpliTube 5, the layout model),
`image4.png`, `image5.png`, `image6.png` (current `sushidsp_host_gui.exe` states), all at
the repository root.

## §1 Goal

The current host GUI (`apps/host_gui`) has no coherent layout: the audio-device dialog opens
pinned to a corner, the device list hardcodes a device name instead of enumerating real
hardware, the transport bar sits at the top in an all-text style, and an unused file/folder
browser panel occupies screen space. This redesign brings the main window to an
AmpliTube-5-style layout, removes the hardcoded device entry, automates splitter/mixer
wiring, promotes the cabinet to a first-class rig component, and turns the mini-DAW into a
collapsible panel with a bounded track count.

## §2 Audio-device panel

- Opens **centered** in the main window, not pinned to the top-left corner.
- **Correction (2026-09-10):** the device list was assumed to hardcode a device name
    (`Roland Rubix`) based on what the panel showed during the walkthrough. Code
    inspection found no such hardcoding: `apps/host_gui/main.cpp:211-217` already calls
    `SushiDSP::AsioAudioDevice::enumerate_drivers()` and `apps/host_gui/ui/DeviceSettings.cpp`
    already renders that list live; `Roland Rubix` in the screenshot was a real ASIO
    driver name enumerated from the machine it was captured on, not a literal in source.
    No `"Roland Rubix"` string exists anywhere under `apps/host_gui/`. This item is
    **dropped** — device enumeration is already correct and out of scope for this
    redesign.
- The only remaining work in this section is centering: `DrawDeviceSettings`
  (`apps/host_gui/ui/DeviceSettings.cpp:34`) currently calls
  `ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing)` with no matching
  `SetNextWindowPos`, so it opens at ImGui's default placement. It must instead compute
  a centered position from the current display size and pass it via
  `ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f))` (or
  equivalent), each time the panel transitions from closed to open.

## §3 Main window layout (AmpliTube-5-style)

Reference: `image1.png`.

- **Top-left:** preset controls — save, load, and any other preset actions currently on the
  window (`Save`/`Load`/preset filename field in `image4.png`).
- **Center, split in two:**
  - **Left of center:** the settings panel for the currently selected rig node (amp,
    pedal, or cabinet controls).
  - **Right of center:** the gear browser — a grid of draggable amp/pedal/cabinet
    components, replacing the current narrow list (`image4.png`'s `Search gear` /
    `Distortion` / `Tube Screamer` list).
- **Bottom:**
  - The transport/metronome bar (§5).
  - The rig design (chain) view, with the splitter-style picker at its far left edge,
    matching AmpliTube's left-side routing icons.
- This layout replaces the current arrangement in full; the present default screen
  (`image4.png`) is not incrementally adjusted; it is rebuilt to this structure.

## §4 Splitter / mixer automation

- The user never adds a splitter or mixer node by hand.
- The rig design view's splitter-style picker (§3, bottom) offers the available splitter
  styles. Selecting one:
  1. Inserts the matching splitter node (`sushidsp.splitter`, see
     `modules/routing/README.md`) at that point in the chain.
  2. Inserts the corresponding mixer node (`sushidsp.mixer`) where the parallel branches
     rejoin.
  3. Wires both ends automatically — the branches created by the split, and the rejoin
     into the mixer — with no manual connection step from the user.
- `sushidsp.splitter` and `sushidsp.mixer` (`modules/routing/`) already exist as ordinary
  rig nodes (per `docs/README.md`); this feature is GUI automation over node placement and
  wiring, not a new node type.

## §5 Transport / metronome bar

Reference: `image6.png` (current), `image1.png`'s bottom bar (target style).

- Moves from the top of the window (current) to the bottom, beside/above the rig design
  view per §3.
- Redesigned to prefer icon/symbol controls over text buttons wherever a clear symbol
  exists (record dot, play/stop/loop transport glyphs, a metronome icon for Click), for a
  visually clean, uncluttered bar. Controls with no natural symbol (BPM number entry, time
  signature, tape-speed multiplier) keep numeric/text fields.
- No functional change to transport behavior in this pass — this is a presentation and
  placement change only.

## §6 Mini-DAW panel

- Functionally unchanged from the current REC/Play/Stop/Loop/lane-view behavior
  (`image4.png`).
- Becomes a **collapsible panel**, hidden by default, toggled by one clearly-labeled
  button. Its internal layout is not redesigned in this pass.
- Tracks/lanes are user-addable and user-removable, with a hard cap of **8 tracks**. The
  UI must prevent adding a 9th track (e.g. disable the add-track control at the cap) rather
  than accept and reject it after the fact.

## §7 Remove the file/folder browser panel

- The panel shown in `image5.png` ("Drag a take or a song onto a lane, or c[lick...]" /
  "(no folder)" file browser, currently under the gear list in `image4.png`) is **removed
  entirely** — not hidden, not collapsed. No replacement panel takes its place; the space
  it occupied is reclaimed by the redesigned layout in §3.

## §8 Cabinet as a first-class rig component

- A new `sushidsp.cabinet` entry appears in the gear browser (§3) alongside amps and
  pedals, draggable into the rig chain the same way.
- It loads an impulse response (IR) and convolves it into the signal path — the same role
  the amp-embedded cabinet plays today.
- **Existing mechanism:** `include/SushiDSP/core/ICabinetHost.hpp` defines the cabinet
  contract (`set_cabinet` / `has_cabinet` / `reclaim_cabinets`) that amp node classes
  (`modules/jcm800`, `modules/peavey5150`, `modules/diezel_vh4`, `modules/mesa_mark2c`)
  implement directly today. There is no standalone cabinet `IAudioNode` yet; IR loading
  (`include/SushiDSP/io/Ir.hpp`, `IrLoader.hpp`) and convolution
  (`include/SushiDSP/math/PartitionedConvolution.hpp`,
  `include/SushiDSP/math/StreamingConvolution.hpp`) are owned by whichever amp hosts them.
- **Decision (owner, 2026-09-10):** `sushidsp.cabinet` is the only cabinet path. Amp node
  classes (`modules/jcm800`, `modules/peavey5150`, `modules/diezel_vh4`,
  `modules/mesa_mark2c`) drop their `ICabinetHost` implementation and the built-in
  convolution it drove; `ICabinetHost` becomes the interface `sushidsp.cabinet` alone
  implements. A rig that wants a cabinet places the `sushidsp.cabinet` node after its amp
  explicitly; there is no amp with a cabinet baked in and no dual-path (no scenario where
  both an amp's built-in cabinet and a standalone cabinet node could convolve the same
  signal). This changes each amp's default signal path — an amp rig that relied on the
  built-in cabinet now needs an explicit `sushidsp.cabinet` node added to sound the same,
  which is an intentional, one-time break tracked as part of this redesign, not a
  regression to avoid.

## §9 Out of scope

- No new audio backend or device-enumeration API is introduced; §2 only replaces the
  hardcoded list with an existing backend's enumeration call.
- No functional change to transport/metronome or mini-DAW behavior — both passes are
  presentation/placement/lifecycle (collapsible, track cap) only.
- No change to `sushidsp.splitter` / `sushidsp.mixer` node behavior — §4 automates their
  placement in the GUI, not their DSP.
