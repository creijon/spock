
## Vulkan Features to Explore

- Compute Shaders
- Raytracing
- Multidraw indirect
- Mesh shaders
- Secondary command buffers

## High Priority Features

- Mesh loading and rendering (GLTF)
- Scene representation
- imGUI
- Asset repository:
  - All assets are registered there and linked to their source file.
  - Each asset type can have a mechanism to rebuild after the source file changes.
  - Doing this in a general way could be hard - if the vertex attributes of a model changes, does the shader have to as well?
  - Perhaps just keep it simple and create a file watcher class. DONE

## Lower Priority Features

- Display lists, high priority as soon as you have a scene representation.
- Debug line rendering. DONE
- Geometric primitives. NEEDS TESTING
- VMA integration

## Larger Scope, Specific Features

- Gaussian splatting (see below)
- Contree (64tree) structure for voxels of volumetric textures
- Lighting

### Gaussian Splatting

1. Initial CPU-GPU hybrid sample. DONE
2. Put in a GPU compute stage to do the sorting, still brute-forced rendering with the vertex shader.
3. Full compute-based rendering, with 16x16 tiles.
4. Spatial partitioning of the splat data.
5. 4DGS.

## Fixes/Refactoring

- Rewrite the queue family logic so that it also supports Compute and Transfer.
  - Split this out to a separate class that does all the querying etc.  DONE
  - Modify the Renderer and Presenter to handle this. DONE
  - Perhaps we should rename the Renderer to Device, Machine or something, because it is generic?
    - This is important, because the renderer currently puts everything on the graphics queue.
    - Needs some thought.
    - The Presenter already encapsulates the Renderer to Window logic, so that will make it easier.
- Create the concept of a Shader Pass:
  - A set of GPU commands that all use the same shader loadout.
  - This could be a compute shader, vert and frag or some combination of all of them.
  - Simplifies buffer bindings, using SPIV to populate them.
  - Clean up the interface into creating and managing buffers.\
- Need to have separate the resources from the renderer.  Have some sort of asset registry.
  - Has to allow reloading.
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
