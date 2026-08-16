## High Priority Features

- Compute Shaders
- Mesh Loading and Rendering (GLTF)
- imGUI
- Scene Representation

## Lower Priority Features

- Raytracing
- Display Lists, high priority as soon as you have a scene representation.
- Debug Line Rendering
- Gaussian Splatting
- VMA integration.
- Support for secondary command buffers.

## Fixes/Refactoring

- Split 

- Split the renderer out and have it separate from the framework. UNNECESSARY
- Clean up the code in the Framework that handles window resizing. DONE
- Fences and semaphores aren't stable.  Move all the synchronisation primitives into Presenter. DONE

## Investigations

## Links

- A lot of good information here: https://docs.vulkan.org/tutorial/latest/00_Introduction.html
