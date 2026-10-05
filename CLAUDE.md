# CLAUDE.md

## Getting oriented

When starting work on this project:

1. Load the `sushiskills` skill and follow the SushiSkills rules it names. The ones this
   repository leans on most are `documentation`, `repository-layout`, `project-tools`,
   `source-comments`, `cpp-code-style`, `python-code-style`, `testing`, `commits` and
   `humanizer`.
2. Read `docs/CONTRIBUTING.md` first and follow it.
3. Then read `docs/README.md`, the manual's index, and follow it into what your task touches.
4. `docs/design/SUSHIDSP.md` is the design spec. It records intent, so it may describe work
   that does not exist yet; read it before changing the subsystem it covers, and never treat it
   as a description of the tree as it stands. `build/docs/api-site/`, once `sd docs` has
   generated it, is Doxygen output and is never edited by hand.
5. `docs/design/REMAINING_WORK.md` is the single backlog and `docs/reference/KNOWN_ISSUES.md`
   the list of recorded defects. A task that opens or closes either updates it in the same
   commit.

## How to work

Act as an expert in whatever role the task requires, and work with real engineering discipline,
not a quick hack that happens to pass. One discipline holds across every Sushi Systems
repository: maintainability first, and SOLID without exception.

### Priority 1: Maintainability

Everything must be SOLID and encapsulated within its own boundaries. Every module should be
isolated well enough that:

- Changes stay local instead of rippling across the codebase.
- Anything can be found quickly by someone looking for it.

This decides the project's long-term fate more than anything else, which is why it comes before
performance.

### Priority 2: Performance

The DSP core runs on the real-time audio thread. `IAudioNode::process()`, and everything called
from it, allocates nothing, locks nothing, makes no system call and throws nothing. The
allocation rule is enforced by the trap tests in `tests/functional/unit/test_rt_safety.cpp`;
the other three are held by review. Outside the real-time path, treat wasted cycles as a real
cost: engineer it once carefully, run it a thousand times cheaply.

### Quality bar

"Good enough for now" is not acceptable, least of all in the circuit and solver math, where a
plausible but wrong equation produces audio that sounds fine until it does not.

### Honesty

Say what was completed and what was not. If a task turns out to be too large to finish, never
cut corners and claim it is done. When a design decision narrows scope on purpose, such as
deferring the WDF adaptor tree until a circuit needs it, say so in the plan or the spec.

If a task requires a choice, ask the user. Do not pick arbitrarily.

### Building

Never invoke the underlying build system (`cmake`, `ctest`) directly. Every build and program
action goes through this project's `sd` command line tool: `sd build`, `sd test`, `sd docs`,
`sd check`. `docs/guides/COMMAND_LINE_INTERFACE.md` describes it.

### Task tracking

For every implementation, create tasks up front and update the task list as work progresses.
