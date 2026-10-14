# tests/applications/host_gui/ui

## `test_panel_library.cpp`

### `PanelLibraryReturnsAHandAuthoredPanelForEveryAmplifier`

`// The registry says which products are amplifiers.`

The registry is what says which products are amplifiers, so adding one does not mean remembering
to name it here.

### `PanelLibraryGivesEveryRegisteredNodeAPanelWithAControlPerParameter`

`// Every parameter of every node has a control.`

A node with no hand-authored entry still has to reach every one of its parameters, or the
product ships a knob nobody can turn. Nothing here names a node type, so adding one is a module
and a registry line rather than a test to remember.
