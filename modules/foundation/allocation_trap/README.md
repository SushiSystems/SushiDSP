# allocation_trap

Internal module: the trap the real-time safety tests count heap allocations with. Its headers
are never installed, and no product links it.

It depends on nothing but the standard library.

## allocation_trap.hpp

`SushiDSP::test::AllocationTrap` counts the heap allocations the calling thread makes between
`arm()` and `disarm()`; `count()` reads the total. A case builds one trap, arms it around the
audio-thread calls it checks and expects a count of zero.

`source/allocation_trap.cpp` replaces the global `operator new` and `operator delete`. The
module is an object library, so every test program that links it carries the replacement, and
one program links it at most once.
