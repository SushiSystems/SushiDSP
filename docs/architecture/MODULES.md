# Modules

A module is a product or product-shaped feature: an amplifier, delay, mixer, or capture system. Each registers one or more audio nodes in the rig graph under `sushidsp.*` names. The module's `README.md` documents which class owns which block, the constants and approximations it carries, and any deviations from the design spec.

- **[`modules/nodes/capture/`](../../modules/nodes/capture/)** — recording and playing takes back, the DS phase's own module: no circuit, no `NodeDescriptor`, only the core as a dependency.
- **[`modules/nodes/dd3/`](../../modules/nodes/dd3/)** — the Boss DD-3 delay, `sushidsp.dd3`.
- **[`modules/nodes/diezel_vh4/`](../../modules/nodes/diezel_vh4/)** — the Diezel VH4, one of three high-gain amplifiers sibling to the JCM800.
- **[`modules/nodes/jcm800/`](../../modules/nodes/jcm800/)** — Marshall JCM800 2203, mono in and mono out, registered as `sushidsp.jcm800`.
- **[`modules/nodes/mesa_mark2c/`](../../modules/nodes/mesa_mark2c/)** — the Mesa Mark IIC+, one of three high-gain amplifiers sibling to the JCM800.
- **[`modules/catalog/node_registry/`](../../modules/catalog/node_registry/)** — the product catalog: `node_registry()` and `make_node()`, the one place in the tree allowed to depend on every product module at once.
- **[`modules/nodes/peavey5150/`](../../modules/nodes/peavey5150/)** — the Peavey 5150, one of three high-gain amplifiers sibling to the JCM800.
- **[`modules/session/rig_preset/`](../../modules/session/rig_preset/)** — portable rig presets as JSON, `serialize_rig_to_json()`/`deserialize_rig_from_json()`.
- **[`modules/nodes/room/`](../../modules/nodes/room/)** — one cabinet in one room, `sushidsp.room`: a shoebox solved on the control thread, its direct sound, early reflections, standing waves and late tail rendered on the audio thread, for two ears or one microphone.
- **[`modules/nodes/routing/`](../../modules/nodes/routing/)** — the rig graph's own routing nodes (`SplitterNode`, `MixerNode`, `PanNode`), `sushidsp.splitter`/`sushidsp.mixer`/`sushidsp.pan`.
- **[`modules/nodes/tubescreamer/`](../../modules/nodes/tubescreamer/)** — the Ibanez Tube Screamer, `sushidsp.tubescreamer`, the D3 product that validated the shared DK solver core before JCM800.
