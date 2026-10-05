# Functional tests

The GoogleTest sources of SushiDSP, in two folders.

| Folder | Holds | Reasoning |
| --- | --- | --- |
| `regression/` | One file per node, each comparing a one-second render with a pinned golden file | [`regression/README.md`](regression/README.md) |
| `unit/` | One file per class or module, plus the real-time safety traps in `test_rt_safety.cpp` | [`unit/README.md`](unit/README.md) |

A comment in a test source states one invariant on one line. Where that line needed an
argument, a derivation or a measured figure behind it, the text is in the folder's README under
the test file's name, and the source line ends with the README's path.
