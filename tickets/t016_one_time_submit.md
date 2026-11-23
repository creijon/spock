[t016] oneTimeSubmit() Improvements

### Goal

oneTimeSubmit() improvements, so it doesn't wait at end.

- This could be part of the wider resource registry story.

### Related

t023


### Claude

__Claude:__ (2026-11-19)

Still open. `oneTimeSubmit()` ends with `queue.waitIdle()`, and `Foundry::submit()`, `Foundry::submitCompute()` and the uploads in `wrappers.hpp` all go through it, so every upload stalls until the queue is empty.

Removing the wait means keeping the command buffer and any staging buffer alive until the GPU has finished with them, for example by tracking a fence or timeline value per submit. That ties in with T023 (asset registry).
