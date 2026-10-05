# `apps/host_gui/ui`: the host's widgets and views

Every widget, panel, browser and view the host draws. The units that only do arithmetic
(`ArrangementLayout`, `DividerMath`, `GearFilter`, `KnobMath`, `LevelText`, `SplitLayout`,
`TimelineScale`, `TapTempo`) include no ImGui, so they can be tested without a window.
[`../README.md`](../README.md) describes the host as a whole. The source comments in this
directory cite the sections below.

## `ArrangementView`

`DrawArrangementView` draws the bar ruler, the playhead, the lane headers and the clip canvas
beside each header, into the current window. It replaced `DrawTrackPanel`, so one unit owns a
track's whole appearance. It allocates and it draws, which is why it runs on the control thread
only. `rig` is the rig whose player, clips, transport and musical clock the view drives.
`imports` is the queue a dropped mp3 is sent to, and the view also reports what that queue is
decoding. `geometry_out` exists so that a drop arriving outside the ImGui frame can be resolved
to a lane on the next frame. `header_width` is resized by a divider drawn between the header
column and the grid. The design is
`docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md` §6.

`route_file_drop` is the one route both an ImGui payload drop and an Explorer drop take, so the
two land a file on the same frame. `rig` is the rig the clip is placed on; `scale` and `snap`
are the zoom, the scroll and the rounding the position is read through. A file the tree cannot
read is sent to `imports` for decoding and not loaded, and its clip appears on the frame the
decode finishes. Of `canvas_position`, only the x names a frame. `error_out` receives the
message when the file will not load or the lane refuses the clip.

In a lane header, the track's own color runs down the leading edge. A click on that strip opens
the palette, and a right-click anywhere on the header offers to remove the track. Removing a
track asks what to do with its files every time and never chooses for the person, because one
of the answers cannot be undone.

A clip's ImGui id is the frame it starts on. Clips on a lane may not overlap, so that frame is
unique on the lane, and a reorder retires an open popup where an index would have pointed it at
the neighbor that took the index.

## `DeviceSettings.hpp`

The settings window draws in its own window and does not block input to the windows behind it.
It draws only while `open` is true. Close clears `open`. Connect leaves it set, so a failed
connection keeps the window up to show the error; the caller clears it once the device opens.

## `Divider.hpp`

`DrawDivider` draws a 14 px-thick invisible button spanning the region's cross axis, with a
centered 3x36 px rounded handle, and advances the cursor like a normal item. `available` is the
total space split between this pane and the other one along the resize axis; the strip's own
drawn length follows the current content region.

## `GearBrowser.cpp`

Each of the List and Grid toggles is as wide as its own caption plus padding. A fixed box
narrower than the text lets the two labels overlap each other and the window's edge.

The tab captions are centered in their boxes. A `Selectable` puts its caption in the top-left of
the box it is given, which reads as text that has come loose from its button once the box is
wider than the word.

## `PanelLibrary.cpp`

The Mark IIC+ front panel has the knobs on the left and the graphic equalizer's faders on the
right. The knobs sit in two rows because eight knobs across half of a 900x420 panel crowd.

## `RigChainView.cpp`

A chain with one path is the run itself and carries no master bus, so that path is the only
stage the view shows, and it takes the geometry `layout_split` builds when no branch sizes are
given. A chain with more than one path puts each path in its own row, then the master bus.

## `RoomPanel.hpp`

`DrawRoomPanel` draws a room slot's controls in three layers and a mix group. Shell holds the
shoebox's dimensions and a top-down plan where the cabinet, the head and the microphone drag,
with each pose's height and facing and the microphone's pattern. Finishes holds a material and
a scattering word per surface. Furnishings holds a list of furniture rows with a quantity each.
The mix group holds the node's mix, level, listener and direct-delay parameters.

Beside them the panel reads out the Schroeder frequency the node split its last set at, Weyl's
estimate of the modes under it, and where the HRTF came from. Every edit goes through
`RigController::set_room_scene`, which paces a drag. The design is
`docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md` §9.2 and §14, decision DE6.

## `SplitLayout`

`layout_split` has two shapes. With `branch_sizes` empty it lays out `series_size` nodes between
IN and OUT and leaves `pre`, `post`, `branches`, `splitter`, `mixer` and `cables` empty.
Otherwise it lays out `pre_size` nodes, a splitter glyph, one row per entry of `branch_sizes`
stacked around the block's vertical centre, a mixer glyph, then `post_size` nodes.

A stage has one drop gap before each node and one after the last, so a caller can always drop
at either end. An empty stage collapses to a single gap `empty_slot_width` wide, which keeps an
empty branch a valid drop target.

A cable is routed the way the mockup's `redrawSplitWires` draws it: an orthogonal step from the
source's right edge, halfway across the gap, into the target's left edge.

## `TimelineScale.cpp`

`snap` rounds a frame to the nearer of the grid frames below and above it. `position_of` floors,
so a frame that is itself a grid frame can be placed on the beat before it with a fraction just
under one. `upper` is then that frame and the rounding finds zero distance to it.

## `TopBar`

`DrawTopBar` draws three columns into the current window. The left column holds the preset path
box, Save and Load, which mean something only with a rig, then the last Save or Load status,
elided so it never runs into the brand mark. The centre column is the "SushiDSP" brand mark,
placed at (window width - its width) / 2 whatever either side group drew. The right column
holds the connection pill, or the "No device connected." label when there is no device, and the
settings button, pinned to the bar's right edge. `device_label` names the connected device and
its channel selection.

`rig_replaced` reads true on the frame a Load succeeded. A load rebuilds the slot id space from
zero, so ids that survive the load name different gear; every caller holding a `SlotId` must
drop it on that frame.

## `TransportBar`

`DrawTransportBar` draws, into the current window: record, play, stop and rewind, the loop
switch, the transport position, the metronome's switch, the tempo, the time signature, TAP, the
playback speed, the disabled Pitch field, the Mini-DAW toggle and, pinned to the right, the
input and output level meters. The take folder and name boxes, the rig's input source, the loop
range, the metronome's level and the recorder's dropped-block count are not in the mockup this
bar follows; they stay, wrapped into a second row after Speed. The Mini-DAW button toggles
`mini_daw_active` and reads "Mini-DAW" or "Back to Rig" accordingly. The design is
`docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md` §5a and §6 and
`docs/design/GUI_UX_REDESIGN.md` §12.

A tempo typed by hand ends whatever tap run was in progress, so the next tap opens a run of its
own and does not average across the change.

The speed slider consumes its own clicks. The way back to unity is therefore the "1x" button
beside it; a double-click on the slider would have been swallowed.
