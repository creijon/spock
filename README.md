# Spock

Spock is a small C++ Vulkan framework and sample project built around Vulkan-Hpp RAII wrappers, GLFW, GLM, and glslang. The library is organized as a shared core library plus separate executable samples for different rendering demos.

## Current project structure

```text
spock/
├── CMakeLists.txt
├── README.md
├── deps/
│   ├── glfw/
│   ├── glm/
│   └── glslang/
├── src/
│   ├── CMakeLists.txt
│   ├── samples/
│   │   ├── CMakeLists.txt
│   │   ├── cube.cpp
│   │   └── quad.cpp
│   └── spock/
│       ├── CMakeLists.txt
│       ├── creators.cpp
│       ├── creators.hpp
│       ├── framework.cpp
│       ├── framework.hpp
│       ├── helpers.cpp
│       ├── helpers.hpp
│       ├── math.cpp
│       ├── math.hpp
│       ├── presenter.cpp
│       ├── presenter.hpp
│       ├── shaders.cpp
│       ├── shaders.hpp
│       ├── wrappers.cpp
│       └── wrappers.hpp
└── build/
```

## Build system

The root CMake project defines a static library target named `spock` and then creates sample executables for each demo.

Current targets:
- `spock` — shared core library
- `cube` — standalone cube sample
- `quad` — standalone quad sample

Example:

```bash
cmake -S . -B build
cmake --build build --target cube
cmake --build build --target quad
```

The sample targets link against the shared `spock` library so the reusable framework code is compiled once and reused across demos.

## Core library

The reusable engine code lives under `src/spock` and includes:

- `framework.*` — base application loop and Vulkan framework setup
- `creators.*` — helper functions for creating Vulkan resources and pipeline objects
- `helpers.*` — utility routines for shader and buffer management
- `math.*` — matrix/vector helpers used by the samples
- `presenter.*` — presentation-layer support and render state helpers
- `shaders.*` — runtime GLSL-to-SPIR-V shader compilation support
- `wrappers.*` — RAII wrappers around Vulkan objects such as buffers and descriptors

## Samples

The current demos are in `src/samples`:

- `cube.cpp` — a simple colored cube example using the framework
- `quad.cpp` — a simple quad example built as a separate executable target

## Requirements

- CMake 3.15+
- C++17 compiler
- Vulkan SDK
- OpenGL development libraries
- GLFW
- GLM
- glslang

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

After configuration, the sample binaries are available under the generated build tree, typically in directories such as:

```text
build/src/samples/Debug/
```

or the corresponding build output location for your generator and configuration.


### Compiling Shaders at Runtime

```cpp
glslang::InitializeProcess();

vk::raii::ShaderModule vertShader = 
    spock::makeShaderModule(device, 
                           vk::ShaderStageFlagBits::eVertex, 
                           vertexShaderGLSL);

vk::raii::ShaderModule fragShader = 
    spock::makeShaderModule(device, 
                           vk::ShaderStageFlagBits::eFragment, 
                           fragmentShaderGLSL);

glslang::FinalizeProcess();
```

## Building for Release

To create an optimized release build:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

## Extending Spock

### Adding New Samples

1. Create a new class derived from `spock::Application`
2. Override `update()` and `render()` methods
3. Add your graphics code in `src/main.cpp` or create a new entry point
4. Update `CMakeLists.txt` if creating a separate executable

### Customizing the Application Base

The `Application` class provides hooks for customization:
- Clear colors and depth values
- Window dimensions
- Frame timing
- Render pass configuration

See `src/spock/application.hpp` for all configurable parameters.

## Performance Considerations

- **Double Buffering**: Two frames in flight optimize GPU/CPU synchronization
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

### Application crashes on macOS
Ensure you're using the `Metal` rendering backend (automatically selected on macOS).

## Contributing

Contributions are welcome! Please follow these guidelines:
- Use C++17 standard features
- Follow the existing code style (4-space indentation)
- Add comments to non-obvious code
- Test on at least one platform (Windows, macOS, or Linux)
- Ensure no validation layer errors

## License

This project is licensed under the Apache License 2.0. See `LICENSE` file for details.

## References

- [Vulkan API Documentation](https://www.khronos.org/vulkan/)
- [Vulkan-Hpp C++ Bindings](https://github.com/KhronosGroup/Vulkan-Hpp)
- [GLFW Documentation](https://www.glfw.org/documentation.html)
- [GLM Mathematics Library](https://github.com/g-truc/glm)
- [glslang Compiler](https://github.com/KhronosGroup/glslang)

## Roadmap

Future enhancements planned for Spock:

- [ ] Texture loading and sampling support
- [ ] Model loading (glTF/OBJ)
- [ ] Lighting and shadow rendering
- [ ] Compute shader support
- [ ] Performance profiling tools
- [ ] Asset packaging system
- [ ] ImGui integration for debugging UI

## Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check existing documentation and examples
- Review Vulkan API documentation for lower-level questions

---

**Happy coding!** Start with the spinning cube sample and explore from there. Spock is designed to make Vulkan experimentation accessible while maintaining the flexibility needed for advanced graphics work.
