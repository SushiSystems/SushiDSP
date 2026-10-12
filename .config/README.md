# Configuration files

A file lives here when a tool can be pointed at it by a flag, so it need not sit at the root.

| File | Read by | Call site that passes the path |
| --- | --- | --- |
| `doxygen/Doxyfile` | `sd doxygen` (the `docs` command) | `cli/sushidsp/services/project.py`, `docs()` |
