# Spock

Spock is a small C++ Vulkan framework and sample project built around Vulkan-Hpp RAII wrappers, GLFW, GLM, and glslang. The library is organized as a shared core library plus separate executable samples for different rendering demos.

## Build system

The root CMake project defines a static library target named `spock` and then creates sample executables for each demo.

Current targets:
- `spock` — shared core library
- `cube` — a completely self-contained renderer: the geometry and shaders are defined inline
- `splat` — will become a gaussian splatting example
- `shaderlab` — an interactive fragment shader playground, similar to the shadertoy.com site

Example:

```bash
cmake -S . -B build
cmake --build build --target cube
cmake --build build --target splat
cmake --build build --target shaderlab
```

The sample targets link against the shared `spock` library so the reusable renderer code is compiled once and reused across demos.

## Core library

The reusable engine code lives under `src/spock` and includes:

- `app.*` — base application class: owns the Vulkan context/instance, a `Window`, and the renderer, and runs the main loop
- `window.*` — owns the GLFW window handle and creates its rendering surface from an app-provided Vulkan instance
- `renderer.*` — base renderer class: owns the device, command pool, and render pass; subclass it to add your own pipeline and draw calls
- `presenter.*` — swapchain and per-frame synchronization primitives, owned by a `Renderer`
- `creators.*` — helper functions for creating Vulkan resources and pipeline objects
- `wrappers.*` — RAII wrappers around Vulkan objects such as buffers, images and textures
- `shaders.*` — shader compilation support
- `helpers.*` — Vulkan-specific helper routines (queue selection, image layout transitions, memory allocation, surface/present-mode selection, the debug messenger)
- `utils.*` — small non-Vulkan utilities (logging, checked casts)
- `camera.*` — view/projection matrix helper
- `math.*` — shared GLM include and compiler warning setup

## Samples

The current demos are in `src/samples`:

- `cube.cpp` — a colored cube, using push constants for the model-view-projection matrix
- `splat.cpp` — the same cube, using a uniform buffer and descriptor set for the MVP matrix instead
- `shaderlab.cpp` — a full-screen shader sandbox with live shader recompilation on save (ShaderToy-style)

## Tests

The test suite lives under `src/tests` and uses [Catch2](https://github.com/catchorg/Catch2) (v3, found via `find_package`). It builds as a `spock_tests` executable, controlled by the `SPOCK_BUILD_TESTS` CMake option (`ON` by default).

Tests fall into two groups:
- Plain unit tests (`camera_tests.cpp`, `utils_tests.cpp`, `helpers_tests.cpp`, `shaders_tests.cpp` error paths) — pure logic, no GPU required.
- GPU-backed integration tests, tagged `[gpu]` (`gpu_tests.cpp`, `renderer_tests.cpp`) — these create a real, headless Vulkan instance/device (via `VK_EXT_headless_surface`, so no window or display is needed) and exercise `creators.*`, `wrappers.*`, and a full `Renderer` render/present loop. If no usable Vulkan driver is found, these skip themselves instead of failing.

Build and run:

```bash
cmake -S . -B build
cmake --build build --target spock_tests
./build/binaries/spock_tests
```

Or via CTest, which discovers each `TEST_CASE` individually:

```bash
cd build
ctest --output-on-failure
```

Useful Catch2 command-line options (pass directly to `spock_tests`):
- `./build/binaries/spock_tests "[camera]"` — run only tests tagged `[camera]` (or `[utils]`, `[helpers]`, `[shaders]`, `[gpu]`)
- `./build/binaries/spock_tests "~[gpu]"` — skip the GPU-backed tests
- `./build/binaries/spock_tests --list-tests` — list all available test cases

To skip building the test suite entirely (e.g. for a minimal release build), configure with:

```bash
cmake -S . -B build -DSPOCK_BUILD_TESTS=OFF
```

## Requirements

Installed on your system:
- CMake 3.15+
- C++17 compiler
- Vulkan SDK
- OpenGL development libraries
- Catch2 3 (only needed if `SPOCK_BUILD_TESTS` is `ON`; e.g. `apt install libcatch2-dev` on Debian/Ubuntu)

Bundled as git submodules under `deps/` and built as part of the project — no separate installation needed:
- GLFW
- GLM
- glslang
- efsw

## Platform notes

- Windows builds use Win32 Vulkan platform definitions.
- Linux builds support XCB and Wayland selection via the `VULKAN_HPP_USE_WAYLAND` option.
- The project currently uses a static `spock` library target and sample-specific executables instead of building one monolithic app binary.

## Getting started

```bash
git clone --recursive <repository-url>
cd spock
cmake -S . -B build
cmake --build build
```

After configuration, the sample binaries are placed under:

```text
build/binaries/
```

regardless of generator or build configuration.


## Building for Release

To create an optimized release build:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

## Extending Spock

### Adding New Samples

1. Create a class derived from `spock::Renderer` that overrides `render()` and does whatever pipeline/buffer setup it needs in its constructor.
2. Create a class derived from `spock::App` that overrides `createRenderer()` (to construct your renderer) and `update()` (called once per frame before rendering).
3. Give it a `main()` that calls `spock::runApp<YourApp>(...)` with your app's constructor arguments — it constructs the app, runs it, and reports any exception that escapes.
4. Add a new executable target in `src/samples/CMakeLists.txt` via `add_sample_target(your_sample your_sample.cpp)`.

## Performance Considerations

- **Frames in Flight**: Three frames in flight by default, to overlap GPU and CPU work
- **RAII Overhead**: Minimal—modern compilers optimize away the abstraction
- **Shader Compilation**: Occurs at startup; consider pre-compiling shaders for production
- **Validation Layers**: Disable in release builds for maximum performance

## Troubleshooting

### "Failed to find Vulkan SDK"
Ensure the Vulkan SDK is installed and the environment variable `VULKAN_SDK` is set.

### "GLFW window creation failed"
On Linux, ensure X11 or Wayland development libraries are installed:
```bash
# Ubuntu/Debian
sudo apt install libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### "Validation layer not found"
Validation layers are optional. If not installed, they're automatically skipped. Install the Vulkan SDK with validation layers for debugging.

## References

- [Vulkan API Documentation](https://www.khronos.org/vulkan/)
- [Vulkan-Hpp C++ Bindings](https://github.com/KhronosGroup/Vulkan-Hpp)
- [GLFW Documentation](https://www.glfw.org/documentation.html)
- [GLM Mathematics Library](https://github.com/g-truc/glm)
- [glslang Compiler](https://github.com/KhronosGroup/glslang)
- [efsw Filesystem watcher](https://github.com/SpartanJ/efsw)
