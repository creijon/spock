# Spock

## Design Goals

- Allow rapid iteration: minimal setup for new projects, runtime shader compilation.
- Cross platform: Linux, Windows and MacOS currently supporte (Android to follow).
- Thin abstraction: use Vulkan_Hpp RAII to simplify Vulkan API but not hide it.

Spock is a small C++ Vulkan framework and sample project built around Vulkan-Hpp RAII wrappers, GLFW, GLM, and glslang. The library is organized as a shared core library plus separate executable samples for different rendering demos.

## Samples

There are a set of samples which demonstrate functions of Vulkan and the framework.  These will continue to be expanded.

- `cube` — a completely self-contained renderer: the cube geometry and both shaders are defined inline below, with no external asset or shader files to load.
- `shaderlab` - demonstrates automatic shader compilation and hot-reloading. As an example, the default shaders implement a simple ShaderToy-like interface, where the fragment shader can be experimented with in real-time.
- `instancing` - renders a 16x16 grid of sprites. The vertex buffer contains a single quad and the instance buffer contains position and color data for each instance.
- `splat` - demonstrates instanced rendering and the use of uniform and storage buffers.  It implements a basic form of 3D Gaussian Splatting, derived from the tutorial: “3D Gaussian Splatting in a Weekend” by Benjamin Feldman https://bfeldman.me/3dgs-weekend/  Some of the code is adapted from the original tutorial.

## Building

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
- `camera.*` - a simple orbit camera.
- `creators.*` — helper functions for creating Vulkan resources and pipeline objects
- `helpers.*` — Vulkan-specific helper routines (queue selection, image layout transitions, memory allocation, surface/present-mode selection, the debug messenger)
- `math.*` — shared GLM include and compiler warning setup
- `window.*` — owns the GLFW window handle and creates its rendering surface from an app-provided Vulkan instance
- `presenter.*` — swapchain and per-frame synchronization primitives, owned by a `Renderer`
- `renderer.*` — base renderer class: owns the device, command pool, and render pass; subclass it to add your own pipeline and draw calls
- `shaders.*` — shader compilation support
- `utils.*` — small non-Vulkan utilities (logging, checked casts)
- `wrappers.*` — Wrappers around Vulkan objects such as vertex assembly, buffers, images and textures

## Geometry library

`src/geo` is a small standalone 2D/3D intersection-testing library, used for spatial queries such as triangle-vs-AABB overlap testing (e.g. for Sparse Voxel Octree generation from triangle meshes).

`geo3d::Intersect` provides four interchangeable triangle-vs-AABB tests, benchmarked in `src/tests/geo_intersect3d_tests.cpp` (release build, ns/call):

| Scenario                                   | testSS | testNoBB | test | testAM |
|---------------------------------------------|--------|----------|------|--------|
| Edge crosses the box (the common case)      |  21ns  |   7ns    | 17ns |  64ns  |
| Box straddles the interior, no edge touch   |  32ns  |   28ns   | 38ns |  69ns  |
| Disjoint along the triangle's own normal    |  4ns   |   17ns   | 9ns  |  14ns  |

- `testNoBB` — a novel early-exit algorithm, fastest when queries are mostly intersecting (e.g. SVO generation).
- `testSS` — adapted from Schwarz-Seidel; fastest when queries are mostly disjoint.
- `test` — `testNoBB` with an AABB precheck; a middle ground when disjoint queries are common.
- `testAM` — the standard 13-axis SAT reference (Akenine-Möller); a correctness/performance baseline, not intended for production use.

See the comment above `Intersect::testNoBB` in `src/geo/intersect3d.cpp` for the full breakdown.

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

### MacOS

MacOS requires the MoltenVK SDK for Vulkan support, which can be downloaded from the [lunarg](https://vulkan.lunarg.com/sdk/home) site.

Some of the samples require TBB and OneDPL libraries for parallel sort.  These can be installed with brew:

```bash
brew install tbb onedpl
```

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
