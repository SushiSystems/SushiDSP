# `src` — the core library's implementation

These files implement the headers in [`include/SushiDSP/`](../include/SushiDSP/README.md), folder
for folder. One header lives here and not there: `math/ResamplerKernel.hpp`, the generated kernel
table that `math/Resampler.cpp` includes.

## Notes cited from the source

A comment in these files states one invariant and names this file. The reasoning behind each
invariant follows, under the name of the file that cites it. The two comments in
`core/RtGraphSlot.cpp` cite the header README, where `RtSwapSlot` and `RtGraphSlot` share one
section.

### `audio_format/AsioSampleConverter.cpp`

`float_to_asio_sample` scales an Int16 sample by `kInt16FullScale - 1.0f` in float and an Int32
sample by the same expression in double. At 2^31 a float's ULP is 256, so `kInt32FullScale - 1.0f`
rounds back to `kInt32FullScale` itself, and casting the product to `int32_t` overflows. Promoting
to double first keeps the subtraction exact at `INT32_MAX`, which the cast then truncates
correctly.

### `circuits/FmvToneStack.cpp`

The treble pot's a-to-wiper section grows as the knob closes, so its DK position is one minus the
panel position. The bass and middle controls are rheostats: the wiper carries the section's
current, and the dead wiper-to-b section stamps nothing because its two terminals are one node.

### `circuits/stages/PentodeStage.cpp` and `circuits/stages/TriodeStage.cpp`

Both stages solve the same grid node. With every branch current leaving the node and Vg referenced
to the cathode, Kirchhoff's current law reads

```
(Vg - Vk - Vin) / Rs + (Vg - Vk) / Rg + IG0 * softplus((Vg - VGAMMA) / VTG) = 0
```

The residual is convex and increasing in Vg, so Newton converges from either side. The two grid
bounds handed to the solver mark where the plate ceiling has already saturated.

`root_stays_on_bound` skips the solve when the warm start sits on a bound and would stay there.
Newton's first step from a bound leaves the iterate on it whenever the residual there has the sign
that points past the bound. The conduction current is never negative, which settles the cutoff
bound from the linear part alone, and at the conduction bound softplus equals its argument.

### `core/RigGraph.cpp`

`compile` clears the cached latency, the sink handle and the topological order before it does
anything else. A `compile()` that throws partway through, on a cycle, on several sinks or on none,
can then never leave a stale latency or sink value behind from an earlier successful one.

A node with no upstream edge is a root node. `process` feeds it the graph's own live input, for
instance an ASIO device's captured audio, so a rig can start directly from live input without a
synthetic source node.

The host's own inputs array is never handed to a node directly. Hosts pass a one-element array,
while a multi-input node such as `MixerNode` reads all `kMaxFanInOut` slots. Routing the external
input through slot 0 of the fixed-size gather array means every node sees a full-width array whose
unused slots are `nullptr`, and no node reads past the host's array.

An empty graph must still fully define its output. Leaving the caller's buffer untouched would
replay whatever it last held, SDL's raw stream buffer for one, which is audible garbage looping at
block rate.

### `device/AsioAudioDevice.cpp`

`asiodrivers.cpp` defines `loadAsioDriver` and declares it in no header. The SDK's own host sample
declares it locally too, so a local `extern` is the sanctioned way to reach it.

Once `loadAsioDriver` has succeeded, any failure in the constructor must unload the driver. If it
does not, the process-wide SDK state stays wedged and the next open of any driver fails.

`ASIOInit` wants a window handle in `sysRef`. Console and library hosts have no application
window; the desktop window is the conventional stand-in, and drivers that ignore `sysRef` are
unaffected.

`ASIOCanSampleRate` is the only non-destructive way to ask whether a rate is available. The
constructor sets the rate only when the driver confirms it, so an unsupported request leaves the
hardware's current clock alone and does not fail the whole open.

Every channel in one direction must share a container format. The device keeps a single format
per direction and no per-channel table, so it rejects a mixed-format driver where a table-less
read would misinterpret the samples.

`asio_message` answers the driver's selectors as follows. Resync, latency-change and overload
notices are acknowledged so the driver does not keep re-asking; the device does not act on them,
because reopening the driver is the host application's call. A reset request gets 0. Answering 1
would claim the device tears down and re-creates its buffers in response, which it does not do. A
real reset handler belongs at the host layer that owns rebuilding the format and the buffers, and
that layer does not exist yet. So 0 is the truthful "not handled" answer, and the reset selector
is left out of the `kAsioSelectorSupported` list for the same reason. Time info is declined, which
keeps the driver on the plain `bufferSwitch` path; that path is all the device needs.
`buffer_switch_time_info` is reachable only if a driver ignores that answer, and it forwards the
block so that nothing is dropped.

`sample_rate_did_change` is empty. `format()` reports the rate negotiated at construction, and a
mid-stream clock change is a driver-level event this device does not chase.

### `device/Sdl2AudioDevice.cpp`

Once stopped, the device must not keep a reference to whatever the caller's callback captured.
`SDL_PauseAudioDevice` waits for any in-flight callback to finish, so `stop` clears the callback
right after it.

### `math/Biquad.cpp` and `math/Fft.cpp`

`_USE_MATH_DEFINES` is what makes MSVC's `<cmath>` declare `M_PI`. It must be the first thing in
the translation unit: any earlier include could pull `<cmath>` in first and latch the definition
out.

In `Fft`'s constructor, the stage of a given span reads `exp(-i*2*pi*(k*stride)/size)` for k in
`[0, span)`, with `stride = size/(2*span)`. The spans' tables sit end to end, the table for a span
starting at offset `span - 1`.

### `math/Oversampler.cpp`

`HalfbandUpsampler2x::process` never forms the zero-stuffed signal. With `tap_count = 4M-1`, the
centre index is `c = 2M-1`, and the zero-stuffed input has `x_s[2n] = 2x[n]` with its odd samples
zero. Output sample m only sees taps `h[k]` with k congruent to m modulo 2.

Output `2n+1` needs odd k, an even distance `k-c` from the centre, and every one of those taps is
an exact zero except the centre itself. So `y[2n+1] = 0.5 * x_s[2n+1-c] = x[n - (M-1)]`, a pure
delay of `centre_delay_ = (tap_count-1)/4 = M-1` base-rate samples.

Output `2n` needs even k, an odd distance `d = 2k+1` from the centre, which is exactly the
non-zero pair set. The tap at `c-d` reads `x[n - (centre_delay_ - k)]` and its mirror at `c+d`
reads `x[n - (centre_delay_ + k + 1)]`, so one coefficient serves both and is multiplied once.

The pair sum is accumulated in float. The 1e-6 equivalence with the direct form that the tests pin
is empirical. It holds because these designs' taps decay fast, so a handful of terms dominate the
sum; it is not a worst-case error bound.

`HalfbandDownsampler2x::process` computes only the retained outputs `y[m] = sum_k h[k] u[2m-k]`.
The discarded input samples still enter the history so that it stays continuous. Within a retained
output only the non-zero taps are touched: the centre, at history offset `centre_offset_`, and the
symmetric pairs at odd distances 1, 3, 5 and so on either side of it.

`latency_samples()` adds up the stages. A linear-phase FIR of T taps delays by `(T-1)/2` samples
at its own rate. The up-stages run at 2x, 4x and, in `Oversampler8x`, 8x of the base rate, and the
down-stages at the same rates; each delay is converted to base-rate samples before the sum.

### `math/PartitionedConvolution.cpp`

Each partition is the impulse-response slice `[k*N, (k+1)*N)` zero-padded to 2N. The second half
must stay zero: it is the headroom that turns the FFT's circular convolution into the linear one
the overlap-add reconstruction assumes.

In `process`, the copy of the input block overwrites the whole lower half of the padded scratch,
so only the upper half needs clearing, and that half is already zero on every call after the
first.

The accumulation is `Y = sum_k X[i-k] * H[k]`: impulse-response partition k lines up with the
input block received k calls ago, which is the history slot k steps behind the one just written.

### `math/Resampler.cpp`

`prepare` sizes the history on the slowest ratio, which gives the widest kernel span.
Interpolating never narrows that span below the prototype's own half width, so a minimum ratio
above 1 sizes the history exactly as a minimum ratio of 1 does.

The ring holds `history_.size()` samples ending at the newest input, and `interpolate()` reaches
`half_span_ + 1` below the phase. `set_rates` costs `position_floor_` against `max_input_frames_`
and not against the push in hand. That makes the floor conservative by up to `max_input_frames_`
minus that push: it errs towards dropping a deferred output the ring could still have formed, and
never towards reading a slot the ring has overwritten.

`set_rates` leaves `position_` and the history as they are. A mid-stream rate change is the DD-3's
pitch glide, and resetting the phase there is the click the design exists to avoid.

A `set_rates()` that lowers the ratio widens `half_span_` and so raises the floor, which can leave
a phase the previous call pinned sitting below it. `process` clamps on entry for that reason: the
floor in force is the one the ratio in hand asks for.

An under-sized output buffer stops the output loop without moving `position_` past what was
written, so the next call resumes on the same output and drops nothing. The ring bounds how much
backlog that can carry. Below `position_floor_` the deferred outputs would read history the ring
has overwritten, so they are dropped. Clamping at the end of `process` as well as on entry keeps
the phase bounded across a long starved run.

### `math/ResamplerKernel.hpp`

`tools/dsp/design_resampler.py` writes this file together with
`include/SushiDSP/math/ResamplerDesign.hpp`; a hand edit is lost at the next run. The tool
versions, the measurements and the half-width ruling are recorded once, under `ResamplerDesign.hpp`
in the [header README](../include/SushiDSP/README.md). The same section records how the comments
in the two committed files differ from what the generator writes.

### `math/StreamingConvolution.cpp`

`output_fifo_[fifo_fill_ ..)` is the not-yet-emitted result of the previous partition, so reading
one output sample and writing one input sample keep the same cursor. Starting from a zero
`output_fifo_` is what makes the latency exactly one partition.
