# `applications/host_gui/ui`: the host's widgets and views

Every widget, panel, browser and view the host draws. The units that only do arithmetic
(`ArrangementLayout`, `DividerMath`, `GearFilter`, `KnobMath`, `LevelText`, `SplitLayout`,
`TimelineScale`, `TapTempo`) include no ImGui, so they can be tested without a window.
[`../README.md`](../README.md) describes the host as a whole. The source comments in this
directory cite the sections below.

## `ArrangementView`

`DrawArrangementView` draws the bar ruler, the playhead, the lane headers and the clip canvas
beside each header, into the current window. It replaced `DrawTrackPanel`, so one unit owns a
track's whole appearance. It allocates and it draws, which is why it runs on the control thread
only. `session` is the session whose tracks, clips, transport and musical clock the view drives.
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

Above the ruler sits the project row: the project's path with New, Open and Save, which empty,
read and write the whole session, and Export, which opens a popup for the mix's file and format
and becomes a progress bar while `SessionBounce` runs. New asks first, since what is not saved
is lost.

A track's header holds its name, a rig button, a bypass button, its level in decibels, record
arm, mute, solo and its balance, in three rows that span the header and follow the font's
height. The rig button selects the track, which puts its rig in the rig view, the live input on
it, and the top bar's preset on it. The selected track's header is outlined. The bypass passes the track's audio by its rig,
for a play-along track.

In a lane header, the track's own color runs down the leading edge. A click on that strip opens
the palette, and a right-click anywhere on the header offers to remove the track. Removing a
track asks what to do with its files every time and never chooses for the person, because one
of the answers cannot be undone.

A clip's ImGui id is the frame it starts on. Clips on a lane may not overlap, so that frame is
unique on the lane, and a reorder retires an open popup where an index would have pointed it at
the neighbor that took the index.

## `device_settings.hpp`

The settings window draws in its own window and does not block input to the windows behind it.
It draws only while `open` is true. Close clears `open`. Connect leaves it set, so a failed
connection keeps the window up to show the error; the caller clears it once the device opens.

## `directory_listing.cpp`

`list_directory` returns an empty listing when the file system raises an error on the path. It
logs that as one `WARN` under `K_HOST_GUI_LOG_CATEGORY`, with the cause first and the path
last. A path long enough to be cut at the message limit then still shows why it failed.

## `divider.hpp`

`DrawDivider` draws a 14 px-thick invisible button spanning the region's cross axis, with a
centered 3x36 px rounded handle, and advances the cursor like a normal item. `available` is the
total space split between this pane and the other one along the resize axis; the strip's own
drawn length follows the current content region.

## `gear_browser.cpp`

Each of the List and Grid toggles is as wide as its own caption plus padding. A fixed box
narrower than the text lets the two labels overlap each other and the window's edge.

The tab captions are centered in their boxes. A `Selectable` puts its caption in the top-left of
the box it is given, which reads as text that has come loose from its button once the box is
wider than the word.

## `panel_library.cpp`

The Mark IIC+ front panel has the knobs on the left and the graphic equalizer's faders on the
right. The knobs sit in two rows because eight knobs across half of a 900x420 panel crowd.

## `rig_chain_view.cpp`

A chain with one path is the run itself and carries no master bus, so that path is the only
stage the view shows, and it takes the geometry `layout_split` builds when no branch sizes are
given. A chain with more than one path puts each path in its own row, then the master bus.

## `room_panel.hpp`

`DrawRoomPanel` draws a room's shell. Shell holds the shoebox's dimensions and its openings and
coverings. Finishes holds a material and a scattering word per surface. Furnishings holds a list
of furniture rows with a quantity each. Under them sit the node's direct-delay choice, the
Schroeder frequency the node split its last set at, Weyl's estimate of the modes under it, and
where the HRTF came from.

Where the cabinets, the listener and the microphones stand is the room scene's to edit. The panel
only keeps them inside the shell when a dimension shrinks under one of them, and writes the whole
arrangement through `RigController::set_room_arrangement`, which paces a drag. The design is
`docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md` §9.2 and §14, decision DE6.

## `scene_camera.hpp` and `scene_interaction.hpp`

`SceneCamera` is an orbit camera over the room: it circles a target at a distance and projects a
room point onto a screen rectangle in perspective. `unproject_onto_plane` is the inverse for one
horizontal plane, which is what turns a pointer into a place on the floor or at an item's own
height. The elevation is kept between 0.10 and 1.50 rad, so the camera never looks straight down
or along the floor and both directions always have an answer.

`scene_interaction.hpp` is the arithmetic of the scene's mouse: `pick_item` finds the marker
under the pointer, the nearer one when two share a pixel; `drag_across` moves an item over the
room at its own height; `drag_height` raises or lowers it by a vertical drag scaled by its depth;
`drag_turn` turns it. Each keeps the item 5 cm inside the walls. Neither file includes ImGui, so
`tests/applications/host_gui/ui/test_scene_camera.cpp` checks both against hand-computed coordinates.

## `room_scene_view.hpp`

`draw_room_scene` draws the room through a `SceneCamera`: the floor's grid at one line a metre,
the shoebox's twelve edges, and one marker per cabinet (`C1`..), microphone (`M1`..), placed
furnishing (`F1`..) and the listener (`L`), each with a stem down to the floor and a line along
its facing. A cabinet is also drawn as its box with the rim of each cone on the baffle, and a
placed furnishing as its box. A drag on a marker moves it across the room; with Shift it changes
its height, except a furnishing, which stands on the floor; with Alt it turns it. A drag on
empty space orbits the camera and the wheel zooms. The view writes a moved item back into the
`Room::Arrangement` it was given and says so; the caller hands the arrangement to the rig.

## `scene_solids.hpp`

The scene's solid geometry, with no ImGui in it: `cabinet_box` runs a cabinet's box back from
its baffle, `cone_centre` and `cone_outline` place a cone on the baffle, `furnishing_box` stands
a furniture row's nominal box on the floor, `nearest_cone` says which cone a point is nearest
and where it stands to it, `microphone_on_cone` is its inverse, and `cone_label` names a cone
as someone facing the baffle sees it.
`tests/applications/host_gui/ui/test_scene_solids.cpp` checks each against hand-computed coordinates.

## `mixer_model.hpp` and `mixer_panel.hpp`

The strip of the chain's join is named for what it carries, a `JoinContent` the applier works
out from the paths: "Cabinet" when every path holds a cabinet, "Line" when none does, and
"Cabinet + Line" when the chain has already summed a path with a cabinet and one without. A
path with no cabinet moves no air, so the room never hears it; the pane says so in a line above
the scene.

`mixer_strips` turns the rig's `HiddenSlots` into the list of strips the mixer shows, in drawing
order: each microphone, Room, Listener, Cabinet, Bus, DI and Master. A strip names the slot that
holds its level and where its mute and solo live, which is one input of one bus: a microphone's
are on the bus, or on the Room sum when it is marked room; the listener's, Cabinet's and the
Bus's are on the amp bus; the DI's are on the amp bus or the master bus, by its position. The
function is pure and `tests/applications/host_gui/test_cabinet_section_wiring.cpp` checks it.

`draw_mixer_panel` draws each strip as a column: a level fader, pan and delay for a mono strip or
balance and width for a stereo one, mute and solo, and for a microphone its polarity, its room
flag and a remove button. Cabinet and Bus have no fader, since nothing but a bus input stands
behind them. Levels, pans and flags are written through the rig as they change. The panel
returns a `MixerEdit` for the two edits that change wiring, a changed room flag and a removed
microphone, and leaves them to the caller.

## `cabinet_section_pane.hpp`

`DrawCabinetSectionPane` lays the cabinet section out over the workspace: a row with the Add
microphone button, an Add furniture list that stands the chosen row at the room's centre and
selects it, and the DI path's switch and position, the room scene, an inspector beside it,
and the mixer under both. The inspector shows the selected marker's coordinates and facing. For
a cabinet it adds the choice of 1x12, 2x12 or 4x12. For a microphone it opens with the list of
shipped microphones, each named with how it transduces, such as "Flat reference (condenser)",
and the source of the chosen one's curve, so nobody takes the flat capsule for a dynamic
microphone. The list is `microphone_preset` over `K_MICROPHONE_KIND_COUNT`, so a row added to
the room module's table appears here with no change to the pane. Under it is the place against a
cone: a list of every cone named as seen from the front, and the distance in front of the
baffle, aside of the cone's axis and above it, each in centimetres. Choosing a cone puts the
capsule on its axis, and `microphone_on_cone` turns the three numbers into a pose that faces
the baffle. Under them are the pattern and the proximity switch, which it writes to the
microphone's slot and to the arrangement together. For a
furnishing it shows the place, the turn and the quantity, with a button that removes it. Under them a collapsed Room header opens
`DrawRoomPanel`, whose furnishing rows each carry an "In the room" switch: on, the row stands at
a place, first the room's centre; off, it absorbs evenly. A refused
edit's reason is shown in the row at the top until an edit lands.

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

## `timeline_scale.cpp`

`snap` rounds a frame to the nearer of the grid frames below and above it. `position_of` floors,
so a frame that is itself a grid frame can be placed on the beat before it with a fraction just
under one. `upper` is then that frame and the rounding finds zero distance to it.

## `TopBar`

`DrawTopBar` draws three columns into the current window. The left column holds the rig
preset's path box, Save and Load, which write and read the selected track's rig and nothing of
the project, then the last Save or Load status,
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
input and output level meters. The take folder and name boxes, the loop range, the metronome's level and the recorder's dropped-block count are not in the mockup this
bar follows; they stay, wrapped into a second row after Speed. The Mini-DAW button toggles
`mini_daw_active` and reads "Mini-DAW" or "Back to Rig" accordingly. The design is
`docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md` §5a and §6 and
`docs/design/GUI_UX_REDESIGN.md` §12.

A tempo typed by hand ends whatever tap run was in progress, so the next tap opens a run of its
own and does not average across the change.

The speed slider consumes its own clicks. The way back to unity is therefore the "1x" button
beside it; a double-click on the slider would have been swallowed.
