# Track rigs, export and the first plugin

**Status:** Written, built and tested. Nothing was heard or seen on screen, and no DAW has opened the plugin. PLAN.md holds the results.

## 1. What the owner asked for

1. Make the mini-DAW work as AmpliTube 5's recorder does: a track carries a rig, so eight
   tracks are eight live rigs. Each track's rig is chosen from a preset `.json`. A session
   starts with one track.
2. Save a track and load a track.
3. Export the mix as WAV and as MP3.
4. A first plugin: a format-agnostic adapter around the rig, with a CLAP shell and no other.

Three choices were put to the owner and answered: CLAP only, on an adapter designed for more
than one format; LAME for MP3, as a dependency; the plugin is written by one worker that does
not build, and the session builds it.

## 2. What the manual says

Read from the AmpliTube 5 User Manual, sections 4.1 and 4.1.1.

- The recorder has its own chains, one for each track. The live chain is separate from them.
- Eight tracks. A track's strip has: its name, a control that bypasses the AmpliTube instance on
  that track, for play-along tracks that should not run through gear, a three-band EQ, pan, a
  volume fader with a meter, mute, solo, record enable, and the input that feeds it.
- Audio is dragged onto a track or recorded onto it. Clips are moved, split, cut, copied, pasted,
  deleted, cropped, faded and trimmed in gain.
- Transport: play, record, rewind, loop, punch, a metronome from 60 to 240 BPM, global speed and
  pitch.
- A project is created, saved and opened. The mixdown is exported as 16, 24 or 32-bit WAV, or
  AIFF.

## 3. Where the tree stands

`RigController` is the host's one rig, and it owns the `SessionTimeline`. The timeline holds up
to eight lanes of clips, a transport, a metronome and the take recorders. The device callback in
`main.cpp` feeds the live input, or one lane through the re-amp leg, into the one rig, then adds
every lane to the rig's output through `TrackPlayer`. A lane is audio beside the rig, not audio
through a rig.

What exists and stays: lanes, clips and their edits, the transport, loop, tempo, metronome,
speed and pitch, take recording, the arrangement view.

## 4. Design

### 4.1 A track is a lane with a rig

A new class, `Session`, owns the one `SessionTimeline` and up to eight tracks. A track is:

| Part | Holds |
| --- | --- |
| The lane | Its clips, in the timeline, as today. |
| The rig | A `RigController` of its own, built on the shared timeline. |
| The chain state | The `RigTopology` and `TopologyApplier` the rig view edits. |
| The strip | Level, pan, mute, solo, and the rig bypass. |
| The preset | The path of the `.json` the rig was loaded from, when there is one. |

`RigController` stops owning its timeline: it is built on a timeline it shares. A controller
built without one makes its own, so the tests and a bare rig stand as they are.

The rig view, the cabinet section and the gear browser edit the selected track's controller.
Selecting a track in the arrangement view changes which rig they show. A session starts with one
track, and that track's rig is the rig the host has today.

### 4.2 What a block renders

`SessionRenderer` renders one block, and is the only place that does. The device callback calls
it, and so does the export.

For each track, in lane order:

1. The track's input is its lane's clips at the transport's position. The live input is added to
   the selected track's input, so what is played is heard through the rig being looked at.
2. The input runs through the track's rig. A track whose rig is bypassed, or empty, passes its
   input on.
3. The strip scales and pans the rig's two channels. Mute and solo are worked out on the control
   thread into two gains per track.
4. The result is added to the master.

After the tracks: the recorder takes the block, the metronome is added, the transport advances.

`TrackPlayer` gains a way to render one lane alone, before its fader, so a lane can feed a rig.
Its own gain, pan, mute and solo are no longer read by the host; the strip's are.

The audio thread reads the tracks through a fixed table of eight entries, each an atomic pointer
to a rig and a few atomic numbers. A removed track's rig is retired and freed only after the
audio thread has started two more blocks.

### 4.3 Recording

A take is recorded dry onto the armed track, since the track's rig renders it on playback. The
wet take is no longer placed on a lane.

### 4.4 A track's preset, and a track's file

A track's header shows its rig's preset name and loads a `.json` into it. This is the rig
preset format the host already reads and writes.

A track file, `.sdtrack`, is one JSON document: a format number, the track's name, its strip,
its rig as a rig preset, and its clips as the timeline writes them. Loading one adds a track.
Saving the whole session stays the preset the host writes today, with each track's rig and
strip added to its track row.

### 4.5 Export

`SessionBounce` renders the session offline, on a thread of its own, into a file. It builds
its own timeline and rigs from the session's saved state, so the device keeps running. It
renders from frame zero to the end of the last clip plus the tail, which ends when the output
stays under -90 dBFS for half a second or after ten seconds.

Formats: WAV at 16-bit and 32-bit float, which are the two the existing writer knows, and MP3 through
`Mp3Encoder`, which wraps LAME.

### 4.6 LAME

LAME 3.100 is LGPL, so it is linked dynamically and nothing else. It is fetched as the
release tarball and built as a shared library by a CMake file of ours, since LAME ships no
CMake build. One class, `Mp3Encoder`, in the host, wraps it; no LAME type appears in a header.
`NOTICE.md` lists it and the DLL ships beside the host.

### 4.7 The plugin

`applications/plugin/` holds two bricks.

- `RigAdapter`, which knows no plugin format. It prepares at a sample rate and a block size,
  loads and saves its state as a rig preset, reports its latency and its parameters, and renders
  a block of two channels through one rig.
- A CLAP shell, which maps CLAP's entry, activation, process, state and latency onto the
  adapter.

The first plugin has no editor. Its rig is a preset the standalone host saved: the shell reads
it from a path the plugin's state holds, and the state embeds the preset so a DAW session
recalls it.

CLAP 1.2.2 is MIT, header-only, fetched by tag, private to the plugin target.

## 5. What this does not do

- No three-band EQ on a track's strip.
- No fades, crops or gain trims on a clip, no clipboard, no punch region.
- No AIFF.
- No plugin editor, no VST3.
- The re-amp leg and the input-source choice are not removed from the timeline in this work;
  the host stops using them.
- Eight rigs cost eight times one. Nothing here makes a rig cheaper.

## 6. Corrected by the owner, 2026-10-13

Section 4.4 read "save a track" as a file for one track. The owner meant two other things, and
the tree now follows them:

- **A rig is saved as a preset from the top bar**, as AmpliTube 5's Save Preset saves the
  current setup. The top bar's path, Save and Load act on the selected track's rig alone.
- **The work in the mini-DAW is saved as a project**, as AmpliTube 5's File menu does. The
  mini-DAW has a project row with New, Open and Save for the whole session, and the export.

The track file, `.sdtrack`, and the popup on a track's rig button are removed.
