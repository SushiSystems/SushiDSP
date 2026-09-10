# GUI/UX session notes — 2026-09-10

Raw notes from a live walkthrough of `sushidsp_host_gui.exe`, taken before writing the
design spec. Not a spec itself — see `docs/design/` once one exists for this work.

## Findings

- The "Audio device" panel opens pinned to the top-left corner. It should open centered
  in the window instead.
- ~~The device list is hardcoded (`Roland Rubix` appears as a fixed entry).~~ **Corrected
  after code inspection:** the device list already enumerates real ASIO drivers via
  `SushiDSP::AsioAudioDevice::enumerate_drivers()`; `Roland Rubix` was a real driver name
  on the machine the screenshot was taken on, not a hardcoded string. See
  `docs/design/gui_ux_redesign.md` §2.

## Layout redesign target (AmpliTube 5-style)

Reference: `d:\Projects\sushidsp\image1.png` (AmpliTube 5) as the layout model,
`d:\Projects\sushidsp\image4.png` as the current state to replace.

- Top-left: preset save/load and other preset controls.
- Center: the SushiDSP rig chain itself.
  - Left side of center: settings for the currently selected node.
  - Right side of center: the gear browser (grid of amps/pedals/cabinets to drag in),
    same idea as AmpliTube's right-hand gear grid.
- Bottom: metronome/transport controls, and the rig design (chain) view.
  - The rig design view shows the splitter style at its far left, matching how
    AmpliTube shows the routing/splitter icons at the left of its signal chain.
- Everything currently misaligned (per `image4.png`) needs to be aligned to this
  layout; the current default/main screen is considered unusable as-is.

## Splitter / mixer automation

- Splitter and mixer nodes are never added manually by the user.
- When the user picks a splitter style (from the styles shown at the left of the rig
  design view, per the AmpliTube-style left-side icons), the system automatically
  inserts the matching splitter node **and** its corresponding mixer node, and wires
  the chain end-to-end itself.

## Cabinet as a first-class component

- `sushiDSP.cabinet` becomes its own draggable component in the gear grid,
  alongside pedals and amps — not something bundled invisibly inside an amp.
- It loads an IR (impulse response), same role as today's cabinet convolution.
- Existing mechanism found in the tree: cabinet IR loading currently lives behind
  `include/SushiDSP/core/ICabinetHost.hpp` — a mixin interface (`set_cabinet` /
  `has_cabinet` / `reclaim_cabinets`) that amp node classes implement directly
  (see `modules/jcm800`, `modules/peavey5150`, `modules/diezel_vh4`,
  `modules/mesa_mark2c`). There is no standalone cabinet *node* today; the IR
  convolution is owned by whichever amp node hosts it.
  - Open question for the design doc: whether `sushiDSP.cabinet` wraps
    `ICabinetHost` as its own `IAudioNode` (new registry entry, reusing
    `include/SushiDSP/io/Ir.hpp` / `IrLoader.hpp` and
    `include/SushiDSP/math/PartitionedConvolution.hpp`/`StreamingConvolution.hpp`),
    or whether amp nodes' built-in cabinet stays and the new node is layered
    on top / replaces it. Decided at design time, not now.

## Mini-DAW (REAPER-style) panel

- The current transport/timeline block (`image4.png`: REC/Play/Stop, tempo, lane
  view) stays functionally as-is for this pass.
- Scope for this pass is presentation only: it becomes a collapsible panel, hidden
  by default, opened and closed with one clearly-labeled button — not a redesign of
  its internal layout.

## Transport / metronome bar

- Reference: `d:\Projects\sushidsp\image6.png` — the current top bar (take name field,
  REC/Play/Stop/Loop transport row, and the Click/BPM/TAP/time-signature/tape-speed
  metronome row below it).
- Moves to the bottom of the window (consistent with the AmpliTube-style layout
  already noted above — metronome/transport at the bottom).
- Redesigned to lean on icons/symbols instead of text labels wherever possible
  (e.g. a record dot, play/stop/loop glyphs, a metronome icon) for a cleaner, less
  cluttered look than the current all-text buttons.

## Mini-DAW track count

- Tracks/lanes in the mini-DAW panel are user-addable and user-removable, capped at
  a maximum of 8 tracks.

## Remove the file/folder browser panel

- Reference: `d:\Projects\sushidsp\image5.png` — the "Drag a take or a song onto a
  lane..." / "(no folder)" file browser block under the gear list on the right side
  (visible in `image4.png` below the gear grid).
- This panel is removed entirely, not just hidden.

## Reference

- Screenshots at the time of review: `d:\Projects\sushidsp\image.png`,
  `d:\Projects\sushidsp\image1.png` (AmpliTube 5 layout reference),
  `d:\Projects\sushidsp\image4.png` (current rig/DAW screen).
