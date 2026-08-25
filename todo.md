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

## Investigations

## Links

- Excellent: https://docs.vulkan.org/tutorial/latest/00_Introduction.html
- Android: https://developer.android.com/ndk/guides/graphics/getting-started
