[t023] Asset Registry

### Goal

Need to have separate the resources from the renderer.  Have some sort of asset registry.
- Has to allow reloading.
- All assets are registered there and linked to their source file.
- Each asset type can have a mechanism to rebuild after the source file changes.
- Doing this in a general way could be hard - if the vertex attributes of a model changes, does the shader have to as well?
- Perhaps just keep it simple and create a file watcher class. DONE

### Related

t002
t009
t016


### Claude
