# SushiSkills conformance migration

**Status:** Open — the owner's decisions of RW-3.
**Implements:** SUSHI_STANDARDS_MIGRATION#P2

## Decisions

The owner approved each of these on 2026-10-12 with the recommended option.

| Topic | Decision |
| --- | --- |
| Core tier | The core areas become modules of a new lowest tier, `foundation`: `modules/foundation/<area>/` for `audio_format`, `circuits`, `core`, `device`, `io`, `log` and `math`. |
| Same-tier edges | Declared in `cmake/SushiDSPLayers.cmake`: `circuits -> math`, `io -> math`, `core -> io`, `device -> core`, `device -> audio_format`. |
| Module source folder | `source/`, not `src/`. Private headers stay in it. |
| File names | `snake_case`, one primary symbol per header, named after it. |
| Constants | `K_UPPER_SNAKE`. |
| Interfaces | No `I` prefix: `IAudioDevice`, `IAudioNode`, `ICabinetHost`, `ILogSink`, `IRoomHost` become `AudioDevice`, `AudioNode`, `CabinetHost`, `LogSink`, `RoomHost`. |
| Top-level folder | `apps/` becomes `applications/`. |
| Doxygen file | `Doxyfile` moves to `.config/doxygen/Doxyfile`. |
| Formatting | `.clang-format` and `.editorconfig` at the root; the tree is formatted with the Intel oneAPI `clang-format`. |
| Checkers | `check_docs_truth.py` is vendored from `sushiskills`; `sd check` also runs `check_source_comments.py`. |
| CI | Build and test go through `sd`, not `cmake` or `ctest` directly. |

## Out of scope for this work folder

These change a public interface or a design, so they wait for an owner decision and are
recorded in `docs/design/REMAINING_WORK.md`:

- The error model: exceptions, `bool` and status enums coexist. Moving every fallible function to
  one status shape changes public signatures.
- `[[nodiscard]]` on functions whose return value carries a result.
- Splitting `RigController` and `SlotGraph`, and removing the product-by-id switch in the host.
