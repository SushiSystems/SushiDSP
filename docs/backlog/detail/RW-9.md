# RW-9 Move the cabinet section's gate, or make the amplifier cheaper

The row of this entry is in `docs/backlog/GENERAL.md`. This file holds what did not fit the row.

- State: decision
- Done when: three JCM800 chains into one room with eight microphones meet the gate, or the owner has moved the gate from half the block
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`

The cabinet section at its limits misses its gate. The benchmark row "3 x JCM800 + RoomNode" reads
3.562 ms against a 2.67 ms gate. Cause: three JCM800s cost 2.03 ms and the room 1.28 ms of a
256-frame block.
