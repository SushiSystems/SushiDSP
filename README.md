# SushiDSP

SushiDSP is a C++17 library for developers of guitar audio software: it models amplifiers and
pedals by solving their circuits' nodal equations sample by sample, in real time.

```cpp
    SushiDSP::RigGraph rig(actual.block_size, 4);

    // The node controls are set once here; applications/host/README.md says why.
    auto tube_screamer = std::make_unique<SushiDSP::TubeScreamer>();
    tube_screamer->set_drive(0.7f);
    // ...
    auto jcm800 = std::make_unique<SushiDSP::Jcm800>();
    jcm800->set_preamp(0.7f);
    // ...
    const SushiDSP::NodeHandle source_handle =
        rig.add_node(std::make_unique<SushiDSP::SineToneNode>(440.0f));
    const SushiDSP::NodeHandle tube_screamer_handle = rig.add_node(std::move(tube_screamer));
    const SushiDSP::NodeHandle jcm800_handle = rig.add_node(std::move(jcm800));
    rig.connect(source_handle, tube_screamer_handle);
    rig.connect(tube_screamer_handle, jcm800_handle);

    SushiDSP::PrepareInfo info{actual.sample_rate, actual.block_size, actual.output_channels};
    rig.compile(info);

    device->start(
        [&rig](const float* const* inputs, float* const* outputs, std::uint32_t frames)
        {
            SushiDSP::ProcessContext context{outputs, inputs, frames, 0.0};
            rig.process(context);
        });
```

Source: `applications/host/main.cpp`

## Build

Every build and test goes through `sd`, the Python package in `cli/`. CMake 3.20 or newer, on
Windows and Linux. Standalone, with Git and Python:

```bash
python -m pip install -e ./cli
sd setup
sd build
sd test
```

With SushiHub: `hub add sushidsp` and `hub install-cli sushidsp`, or `hub link sushidsp <path>`
for a checkout you already have. [docs/getting_started/INSTALL.md](docs/getting_started/INSTALL.md)
has the prerequisites for both, and
[docs/guides/COMMAND_LINE_INTERFACE.md](docs/guides/COMMAND_LINE_INTERFACE.md) every command.

## Status

Six circuits are modelled: the Marshall JCM800 2203, the Mesa/Boogie Mark IIC+ in lead mode, the
Peavey 5150 and the Diezel VH4, the Ibanez Tube Screamer and the Boss DD-3, with cabinet
convolution, room acoustics, a standalone host GUI and a CLAP plugin beside them. The Mark IIC+
has no clean setting and the VH4's power section is a stand-in; those and every other open item
are in [docs/design/REMAINING_WORK.md](docs/design/REMAINING_WORK.md) and
[docs/reference/KNOWN_ISSUES.md](docs/reference/KNOWN_ISSUES.md).

## Documentation

[docs/README.md](docs/README.md) is the manual index. Read
[docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) before changing anything.

## Licence

PolyForm Noncommercial License 1.0.0, see [LICENSE](LICENSE). Commercial use needs a licence
from Sushi Systems, see [COMMERCIAL.md](COMMERCIAL.md). [NOTICE.md](NOTICE.md) lists third-party
code and data, the ASIO SDK terms and the trademark position.
