[t001] Render Stages and Renderer Refactor

#### Goal

#### Claude

__Me:__

I think a better design than the current prepare() render() finish() functions is to have a set of RenderStages in a vector.  Would that be more flexible or would it overcomplicate the design?


__Claude:__

I'd hold off. With the samples you have now, a vector of `RenderStage`s would make the design more complicated without letting you do anything new. The hooks aren't what limits flexibility; the single render pass that `Renderer` sets up is.

**How the hooks are used today**
- **`render()`:** 11 of the 13 sample renderers override only this.
- **`prepare()`:** only two samples use it, [shaderlab_compute.cpp:320](samples/src/shaderlab_compute.cpp#L320) and [video_player_hw.cpp:327](samples/src/video_player_hw.cpp#L327). Both use it for barriers and semaphore waits ahead of a single draw.
- **`finish()`:** only the hook-order test overrides it.

So the current hooks already cover every sample. A vector would just put the same fixed sequence into a different shape.

**Why a plain vector doesn't add much**
1. **There's still only one render pass.** `Renderer` owns one `VkRenderPass` that draws to the swapchain, plus its framebuffers and depth buffer, and pipelines are built against `m_renderPass`. Stages are only worth having if each can draw to its own target, like a shadow map, an offscreen pass or a post-process pass. That means each stage owns a render pass and framebuffers, rebuilds them on resize, and builds its pipelines against its own pass. Because you target Vulkan 1.2, you don't get core dynamic rendering, which would have made this easier. That's where the real work is, and the vector does none of it.
2. **The per-frame state is shared.** Each sample subclasses `FrameState` and uses `static_cast` to get its own type back. In `shaderlab_compute`, the compute half and the draw half share the per-frame image, descriptor sets and timeline. Split them into two stages and both still cast to `ShaderLabComputeFrameState`, so they're separate in name only. For stages to be reusable, each would need its own per-frame storage, which is a new mechanism in the pool.
3. **List order doesn't describe dependencies.** Stage B still has to assume the layout and queue owner that stage A left the image in, and record the barriers by hand. The natural next step is declaring resources so barriers are generated automatically, and that's a render graph, which is much bigger.
4. **Some stages don't record into the frame's command buffer.** The compute stage records and submits its own command buffer on another queue. A stage interface would need to cover both cases, so it gets wider than it looks at first.

**When it would be worth it**
It becomes worth it once you have a sample that needs two or more passes, or a pass you want to reuse across samples, such as a UI or debug-lines overlay. Even then, I'd start with a smaller version:

- `Renderer` keeps its swapchain pass and `render()`.
- `prepare()` and `finish()` are replaced by an ordered list of stages that run before and after the main pass.
- Each stage gets a per-frame slot of its own, so it doesn't need to know the app's `FrameState` subclass.

That gives you composable passes without moving framebuffer ownership or pipeline creation. You'd only make the swapchain pass a stage too once you need offscreen targets that feed it.

Until then, one easy cleanup is dropping `finish()`, which nothing uses. That said, it costs almost nothing to keep, and it's the natural place for the release half of a queue-ownership transfer.


__Me:__

OK, if I move the system to a more generic compute design I'll do it.  I could introduce a new class above the Renderer that manages it.


__Claude:__

Putting it in a class above `Renderer` makes sense. When you get to it, the main thing to sort out is which class owns the frame loop:

- **Per-frame sync would move up.** Right now `Renderer` owns the `FrameStatePool`, and that pool decides when a frame's resources can be reused. Compute-only work needs frames in flight too, but has no swapchain. So the new class would naturally own the pool and the per-frame sync. `Renderer` would shrink to the stage that acquires a swapchain image, records the main pass and presents.
- **`compute_sort` would benefit.** Its `render()` is empty, and its comment says it "should make a headless renderer". With the class above `Renderer`, it could run with no `Renderer` at all.

I've saved a note to memory that the hooks stay for now and that stages would go in a class above `Renderer`, so future sessions won't suggest the refactor unprompted.


__Me:__

Yes, the Presenter would take more responsibility.


__Claude:__

Yes, that fits well, because the `Presenter` already owns most of what's tied to the swapchain: the image views, the framebuffers, `acquireFrame()` and `presentFrame()`.

The rest of the swapchain-sized state still lives in `Renderer`:
- **Depth buffer and render pass:** `m_depthBuffer` and the `m_renderPass` that targets the swapchain.
- **Rebuild and teardown:** `buildSwapchain()` and `releaseSwapchainResources()`.
- **Resize and surface changes:** `resizeWindow()`, `releaseSurface()` and `setSurface()`, which depend on the two functions above.

If those moved into the `Presenter`, it would become the self-contained "draw to the window" end of the frame. `Renderer` would be left recording commands, and the new class above both could drive a frame with or without a window.

I've added this to the memory note as well.