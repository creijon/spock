## Technical Features to Explore

### Vulkan

- VMA integration - P0 importance
- Secondary command buffers
- Multithreaded dispatch
- Multidraw indirect
- Mesh shaders


### Other Features

- Mesh loading with GLTF for Contexture
- Scene representation
- imGUI for general UI help
- Camera controller
- Widgets for moving objects in a scene 


## Fixes

### Priority:

- Per-frame resources are overwritten while the GPU may still be reading them. (See below).

### Unsorted:

- Vulkan version

  - Query the supported instance version

- Presenter error handling around VK_ERROR_SURFACE_LOST_KHR etc

- BufferWrapper::map() doesn't need eHostCoherent

- oneTimeSubmit() improvements, so it doesn't wait at end.

  - This could be part of the wider resource registry story.

- createGraphicsPipeline creates a new pipeline cache for each call, which makes it useless.

- some mechanism to get golden images out of the renderer for tests

- No persistent debug messenger is created.  The callback is only chained into InstanceCreateInfo, so it only covers vkCreateInstance/vkDestroyInstance, and the message filtering in debugUtilsMessengerCallback never applies after that.  makeDebugUtilsMessengerCreateInfoEXT() is unused.

- Window scroll offset is never reset after a wheel event, so the splat camera keeps zooming every frame (and re-sorts every frame) after a single scroll.

- TextureWrapper linear-tiling path ignores the image rowPitch and writes tightly packed rows, so textures skew when rowPitch != width * 4.

- Buffer mapping is inconsistent.

  - upload() and copyToDevice() call mapMemory() even when the buffer is already persistently mapped via map(), which is invalid.
  - The vector upload() and copyToDevice() don't flush, so they are wrong for non-coherent memory.

- findMemoryType() only asserts when no memory type matches, so release builds allocate with type index ~0.  Probably goes away with VMA.

- setImageLayout() assumes an eGeneral source layout was written by the host, so the barrier is wrong after compute or transfer writes.

### Done:

- Transfer queues should be exposed DONE

- Physical device selection DONE

  - Enumerate devices
  - Check against requested extensions and features
  - Check for queues
  - Score based on hard and soft requirements
  - Report on the device selected

  This also will include fixing the extension negotiation.

## Refactoring

- Create the concept of a Shader Pass:
  - A set of GPU commands that all use the same shader loadout.
  - This could be a compute shader, vert and frag or some combination of all of them.
  - Simplifies buffer bindings, using SPIV to populate them.
  - Clean up the interface into creating and managing buffers.

- Need to have separate the resources from the renderer.  Have some sort of asset registry.
  - Has to allow reloading.
  - All assets are registered there and linked to their source file.
  - Each asset type can have a mechanism to rebuild after the source file changes.
  - Doing this in a general way could be hard - if the vertex attributes of a model changes, does the shader have to as well?
  - Perhaps just keep it simple and create a file watcher class. DONE

- Rewrite the queue family logic so that it also supports Compute and Transfer.
  - Split this out to a separate class that does all the querying etc.  DONE
  - Modify the Renderer and Presenter to handle this. DONE
  - Perhaps we should rename the Renderer to Device, Machine or something, because it is generic? DONE
    - This is important, because the renderer currently puts everything on the graphics queue.
    - Needs some thought.
    - The Presenter already encapsulates the Renderer to Window logic, so that will make it easier.
    - This logic is now split off to the "Foundry".

- Support a headless Foundry without a surface or swapchain.
  - For compute-only work, tests and offline rendering (e.g. golden images).
  - Make the surface optional, and only require graphics and present queues, the swapchain extension and createSwapchain() when there is one.
  - The tests currently work around this with VK_EXT_headless_surface.

- Split the framework into an app and separate renderer. DONE
  - App is responsible for: window, update loop, asset loading (TBD)
  - Pure virtual function to create the subclassed renderer.
  - It also needs the vk::Context and vk::Instance because of the way that windows are handled
  - Detects window resizes and forces renderer to rebuild swapchain
  - Renderer has all the GPU resources
  - Presenter belongs to the renderer and manages the swapchain and synchronisation
  - Try to keep the Renderers stateless

- Clean up the code in the Framework that handles window resizing. DONE

- Fences and semaphores aren't stable.  Move all the synchronisation primitives into Presenter. DONE

## Possibilities

- A shared Viewer application, rather than having all the samples be their own application.
  - Reduces code duplication
