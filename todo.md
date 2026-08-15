## High Priority Features

- Compute Shaders
- Mesh Loading and Rendering (GLTF)
- imGUI

## Lower Priority Features

- Raytracing
- Scene Representation
- Display Lists, high priority as soon as you have a scene representation.
- Debug Line Rendering
- Gaussian Splatting
- VMA integration, really an optimisation.

## Fixes/Refactoring

- Clean up the code in the Framework that handles window resizing.
- Fences and semaphores aren't stable.  Move all the synchronisation primitives into Presenter.
- Split the renderer out and have it separate from the framework.

## Investigations

## Links

- A lot of good information here: https://docs.vulkan.org/tutorial/latest/00_Introduction.html

