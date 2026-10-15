# tests/foundation/core

## `test_denormal_guard.cpp`

### `decay_toward_zero`

`// A volatile read stops the compiler folding the product.`

Reading through a volatile keeps the compiler from folding the product at compile time, where it
would be evaluated under IEEE rules regardless of the runtime FPU mode.

## `test_rt_graph_slot.cpp`

### `CanaryNode`

`/// @brief Declares a node whose virtual ports() returns a fixed, checkable value.`

A minimal node whose ports() is a virtual call returning a fixed, checkable value. Poking it
after the owning RigGraph has (incorrectly) been freed exercises the object's vtable pointer --
freed memory that's since been reused for something else will very likely make this crash
outright (invalid/foreign vtable) rather than just quietly return stale data, which is a much
stronger use-after-free signal than a bare null-check on the RigGraph\*.

### `RtGraphSlotConcurrentPublishAndAcquireDoesNotRace`

`// A virtual call catches a dangling graph pointer.`

Actually dereference the returned graph, through a virtual call, rather than just null-checking
the pointer -- a dangling-but-non-null RigGraph\* sails straight through a null-check. Every
published graph here carries exactly one CanaryNode, so nodes() must come back with exactly one
handle, and that node's ports() (a virtual call) must always report { 1, 1 }. If
acquire_for_audio_thread() ever hands back a pointer to a graph reclaim() already freed, this
either crashes outright (the vtable slot now belongs to whatever reused the memory) or the
checks below fail.

## `test_rt_swap_slot.cpp`

### `Counted`

`/// @brief Declares a payload whose destructor is observable through a live count.`

A payload whose destructor is observable, so the deferred-reclaim contract can be checked by
counting frees rather than by inspecting RtSwapSlot's internals. The canary value is
dereferenced (not just null-checked) by the concurrent case: a dangling-but-non-null pointer
sails straight through a null-check, whereas reading a field of freed memory that has since been
reused shows up as a wrong value or a crash.

### `RtSwapSlotReclaimDestroysRetiredObjectsOnlyOnceAudioThreadHasMovedPast`

`// Generation 1 stays alive while the thread is on it.`

The audio thread is still parked on generation 1, so generation 1's object must stay alive no
matter how often reclaim() runs.

### `RtSwapSlotActiveForPublisherDoesNotAdvanceTheAudioThreadWatermark`

`// Reading the active object must not move the watermark.`

active_for_publisher() exists so the publishing thread can look at the active object without
claiming the audio thread reached it. Reading it must therefore leave reclaim()'s "provably
moved past" bound exactly where it was - if it forged the watermark instead, reclaim() would
free the object an audio thread is still using.

`// The read alone moves nothing.`

The audio thread's last recorded generation is still the second acquire above, so the first
object is genuinely retired here. Re-read it through the publisher accessor and confirm that
read alone did not move anything.

`// Only object 1 may be freed here.`

The audio thread has not acquired since generation 2 was published, so reclaim() may free
nothing published at or after it: object 2 (still the generation the audio thread last recorded)
and object 3 (active) must both survive. Only object 1 may go.

`// Only acquire_for_audio_thread() moves the watermark.`

Once the audio thread does acquire, the earlier object becomes reclaimable - proving the
watermark was still being driven by acquire_for_audio_thread() alone.
