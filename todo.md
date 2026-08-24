## Vulkan Features to Explore

- Compute Shaders
- Raytracing
- Multidraw indirect
- Mesh shaders
- Secondary command buffers

## High Priority Features

- Mesh Loading and Rendering (GLTF)
- Scene Representation
- imGUI
- Asset repository (I haven't seen this before on small engines)
  - All assets are registered there and linked to their source file.
  - Each asset type can have a mechanism to rebuild after the source file changes.
  - Doing this in a general way could be hard - if the vertex attributes of a model changes, does the shader have to as well?

## Lower Priority Features

- Display Lists, high priority as soon as you have a scene representation.
- Debug Line Rendering
- VMA integration

## Larger Scope, Specific Features

- Contree (64tree) structure for voxels of volumetric textures
- Gaussian Splatting
- Lighting

## Fixes/Refactoring

- Split the renderer out and have it separate from the framework.
  - Framework is responsible: window, update loop, asset loading
  - Then calls the renderer::render
  - Renderer has all the frame buffers
  - Presenter belongs to the renderer and manages the swapchain and synchronisation

- Clean up the code in the Framework that handles window resizing. DONE
- Fences and semaphores aren't stable.  Move all the synchronisation primitives into Presenter. DONE

## Investigations

## Links

- Excellent: https://docs.vulkan.org/tutorial/latest/00_Introduction.html
- Android: https://developer.android.com/ndk/guides/graphics/getting-started
