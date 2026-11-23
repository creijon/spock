[t024] Headless Foundry

### Goal

Support a headless Foundry without a surface or swapchain.
- For compute-only work, tests and offline rendering (e.g. golden images).
- Make the surface optional, and only require graphics and present queues, the swapchain extension and createSwapchain() when there is one.
- The tests currently work around this with VK_EXT_headless_surface.

### Related

t001
t018


### Claude
