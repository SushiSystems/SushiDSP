# Bundled cabinet impulse response

`sushidsp_host_gui` looks for `assets/ir/default_4x12.wav` next to its executable on startup and
installs it on every JCM800 slot it creates. The build copies this directory beside the
executable, so dropping the file here is enough.

## Requirements for `default_4x12.wav`

- A 4x12 guitar cabinet impulse response.
- Licensed CC0, or under a licence compatible with this project's Apache License 2.0 — the file
  ships inside the repository, so anything with a non-redistributable or non-commercial clause
  cannot go here.
- WAV, PCM 16/24/32-bit or 32-bit IEEE float, mono or stereo (stereo is downmixed).
- Any sample rate: `SushiDSP::IrLoader` resamples to the host rate, truncates to a bounded
  length with a raised-cosine fade-out, and normalises the mid-band level.

Record the source and its licence here when you add the file.

## Not present?

The host prints one warning line to stderr and runs without a cabinet. Any IR can still be
loaded at runtime from the "Cabinet IR path" field on a JCM800 slot, and the path is saved into
the preset.
