# tests/nodes/cabinet

## `test_cabinet_node.cpp`

### `CabinetNodeInstallsAndAppliesAnImpulseResponse`

`// An identity IR makes the cabinet a one-partition delay.`

An identity IR makes the cabinet a pure delay of one partition; every sample that has seen the
delayed input by now must reproduce it exactly.

### `CabinetNodeSurvivesVariableBlockSizes`

`// An identity IR must survive any host block size.`

StreamingConvolution re-blocks the stream internally, so the cabinet must survive a host that
hands the node a different frame count every callback - including counts that are not powers of
two and not multiples of the partition. An identity IR makes the whole cabinet a pure delay of
exactly one partition, so the processed stream must reproduce the raw input stream shifted by
latency_samples(), sample for sample.
