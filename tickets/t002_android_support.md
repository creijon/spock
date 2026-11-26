[t002] Android Support

### Goal

Build and run the spock samples on Android, starting with cube.

### Related

t001
t014
t015
t023


### Claude

**Already in place**
- **Window loss:** `Controller::releaseSurface()` and `setSurface()` drop and rebuild the `Presenter` when the window goes away and comes back. The `Presenter` holds everything sized to the window: the swapchain, depth buffer and framebuffers. The device, renderer, render pass, pipelines, buffers and frames in flight are kept, and tests cover this (T001).
- **Device:** the `Foundry` doesn't keep the surface, so the device survives a surface change.
- **Instance extension:** `app.cpp` enables `VK_KHR_android_surface` when `VK_USE_PLATFORM_ANDROID_KHR` is defined.
- **Rotation, swapchain side:** the swapchain takes the surface's current transform and swaps width and height for 90° or 270°. It exposes the transform through `Presenter::preTransform()`.

**To do, roughly in order**

1. **Build**
   - Build `libspock` and one sample with the NDK, and package them as an APK.
   - Leave GLFW out of the Android build.
   - TBB, used by the samples, and efsw, used by `FileWatcher`, either build with the NDK or become optional. Hot reload doesn't apply on a device.
   - Package the validation layer in debug APKs. `createInstance` already enables it when it's present.

2. **Platform layer**
   - **Window:** write an Android `Window` on `ANativeWindow`. Its surface is created on `APP_CMD_INIT_WINDOW` and destroyed on `APP_CMD_TERM_WINDOW`, so it has to be optional rather than created in the constructor.
   - **Event loop:** replace the GLFW loop in `App::run` with the Android event loop. It stops rendering while there's no window, and calls the `Controller`'s `releaseSurface()` and `setSurface()` on the window events.
   - **Foundry and Controller:** create them on the first `APP_CMD_INIT_WINDOW`, since both need a surface. `App::run` currently creates them at startup from `m_window.surface()`.
   - **Entry point:** an `android_main` that calls `runApp`.
   - **Input:** map touch onto the mouse and scroll API, or replace it.
   - **Logging:** send `writeLog` to logcat.

3. **Rotation, rendering side**
   - **Output:** renderers rotate their output by the `Presenter`'s `preTransform()`, probably through a helper that turns it into a matrix applied after the projection. The `Controller` would pass it to the renderer each time it creates a `Presenter`, as it does the extent with `setExtent()`.
   - **Aspect ratio:** use the window's orientation, which is the swapchain extent with width and height swapped back for 90° or 270°. The viewport, scissor, framebuffers and depth buffer stay in the swapchain's orientation.
   - **Input coordinates:** these need the same rotation, for example `windowToImage()` in `shaderlab_compute`.
   - **Detecting rotation:** on Android 10 and later, present returns `eSuboptimalKHR` after a rotation, and `App::run` already rebuilds on that. A 180° turn doesn't change the size, so it depends on this signal. Older versions need to poll the surface capabilities.

4. **Assets**
   - Samples load files from absolute `SPOCK_DIR` paths baked in at compile time. On Android, files come from the APK through `AAssetManager`.
   - `Loader::texture` uses `stbi_load` on a path, and `loadShader` uses `std::ifstream`. Both need to read from the asset manager instead.

**Open questions**
- **Minimum device:** `selectDevice()` rejects devices below Vulkan 1.2, which rules out many older phones. Keep 1.2, or lower it?
- **Minimum Android API level:** Vulkan needs API 24. The rotation signal needs API 29 (Android 10).
- **Activity type:** NativeActivity or GameActivity?
- **Samples:** which ones to port? The FFmpeg video players are probably out of scope.
