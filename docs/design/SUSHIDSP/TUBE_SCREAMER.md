# The Tube Screamer overdrive

One composite `AudioNode` (`TubeScreamer`) modeling the TS808/TS9 signal path: input buffer stage, an op-amp soft-clipping stage with a diode feedback pair, and a passive tone/output stage, all via WDF/DK-method on the same solver as JCM800 (the circuit-solving core), at the same 8× oversampling. Exposes `tube_screamer_descriptor()`.

```
DI in ─▶ input buffer (JFET/op-amp, unity gain) ─▶ high-pass (input cap)
      ─▶ [op-amp clipping stage: gain network + diode-pair feedback loop, Drive-controlled]
      ─▶ [passive tone stack: single Tone pot RC network]
      ─▶ output level (Level control) ─▶ out
```

- **Op-amp clipping stage:** the core TS nonlinearity, an inverting op-amp with a parallel diode pair (2× silicon diodes, antiparallel) in the feedback path; modeled as a WDF sub-tree with the diode pair as the nonlinear root, solved by the shared Newton–Raphson solver (the circuit-solving core). `drive` sets the feedback resistance (a potentiometer), directly setting the clipping threshold and asymmetry.
- **Tone stack:** single-pot passive RC tone network (the characteristic TS "tone" shaping, a fixed low-pass/high-pass blend) via DK-method, matching the real circuit's frequency-dependent interaction with the pot position.
- **Input/output buffering:** unity-gain buffer stages modeled as linear WDF elements (no nonlinear cost).

**Parameters (`NodeDescriptor`):** Drive, Tone, Level. All smoothed, click-free. **Cost target:** one Newton–Raphson nonlinear stage × 8×, **< 0.2 ms per 256-block on one core** (far cheaper than JCM800, one nonlinear stage vs. ~6).

## As built in D5b

**Parameters** are four, not three: Drive, Tone, Level (dB), Input trim (dB), indexed via
`TubeScreamer::set_parameter(std::uint16_t, float)`, each a `SmoothedParameter` stepped once per
block. Level and Input trim are calibrated in decibels the same way as the JCM800's Master/Input
trim/Output; defaults land the pedal's output around −12 dBFS for a −12 dBFS input. `drive` still
sets the clipper's feedback resistance directly. No cabinet stage; `latency_samples()` reports
only the oversampler's latency. Level is the one knob applied per sample rather than once per
block, so its smoothing time does not depend on the host's block size.
