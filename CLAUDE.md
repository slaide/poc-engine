# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

- `make` - Build everything (automatically clones and builds Podi dependency and compiles shaders)
- `make shaders` - Compile GLSL shaders to SPIR-V using glslangValidator
- `make debug` - Build with debug symbols and DEBUG flag
- `make release` - Build optimized release with NDEBUG flag
- `make clean` - Clean build artifacts (keeps dependencies, removes compiled shaders)
- `make clean-all` - Clean everything including dependencies
- `make run` - Build and run the basic example with proper library paths
- `./run.sh` - Alternative way to build and run the basic example

## Architecture Overview

POC Engine is a C23 graphics framework that provides a cross-platform abstraction layer:

```
┌─────────────────┐
│   Your App      │
├─────────────────┤
│   POC Engine    │  (Graphics abstraction in src/)
├─────────────────┤
│      Podi       │  (Window management dependency)
├─────────────────┤
│ Vulkan | Metal  │  (Platform-specific backends)
└─────────────────┘
```

### Key Components

- **Main Engine** (`src/poc_engine.c`): Core initialization and platform detection
- **Vulkan Backend** (`src/vulkan_renderer.c/h`): Linux graphics implementation
- **Metal Backend**: Planned for macOS (not yet implemented)
- **Public API** (`include/poc_engine.h`): Single header for all engine functionality

### Platform Support

- **Linux**: Vulkan backend with X11/Wayland support via Podi
- **macOS**: Metal backend planned but not implemented
- **Architectures**: x64 and ARM64 on both platforms

### Dependencies

- **Podi**: Cross-platform window management library (auto-cloned from https://github.com/slaide/podi.git)
- **cglm**: Math library for graphics (in deps/cglm, auto-managed submodule)
- **Lua 5.4**: Embedded scripting language (auto-cloned from https://github.com/lua/lua.git)
- **Teal**: Type-safe Lua dialect compiler (single file in deps/teal/)
- **Platform Libraries**:
  - Linux: Vulkan, X11, Wayland, xkbcommon
  - macOS: Cocoa, Metal, MetalKit frameworks

### Build System

The Makefile automatically:
- Detects platform and architecture
- Sets appropriate compiler flags and platform defines
- Clones and builds Podi dependency if needed
- Builds Lua 5.4 as a static library
- Links examples against the engine and platform libraries
- Compiles GLSL shaders to SPIR-V using glslangValidator

### Development Workflow

1. Place new examples in `examples/` directory
2. Engine source goes in `src/`
3. Public headers in `include/`
4. GLSL shaders go in `shaders/` directory (compiled automatically)
5. Lua/Teal scripts go in `scripts/` directory
6. Use `make run` for quick testing
7. Use `PODI_LOCAL_DIR=/path/to/podi make` for local Podi development

### Shader Development

- Place GLSL shaders in `shaders/` directory with `.vert` and `.frag` extensions
- Shaders are automatically compiled to SPIR-V during build using glslangValidator
- Current implementation includes triangle and cube shaders for rendering
- Shaders are loaded at runtime from the compiled `.spv` files

### Scripting System

POC Engine includes an embedded Lua 5.4 scripting system with optional Teal type checking:

#### Lua/Teal Integration
- **Lua Scripts**: Place `.lua` files in `scripts/` directory for immediate execution
- **Teal Scripts**: Place `.tl` files in `scripts/` for type-safe scripting with compile-time checks
- **Type Definitions**: `scripts/poc_engine.d.tl` provides full engine API types for Teal
- **Examples**: See `scripts/examples/` for camera controllers and input handlers

#### Available APIs
- **Core Engine**: `POC.get_time()`, `POC.sleep(seconds)`
- **Camera System**: Create and control FPS, orbit, and free cameras
- **Input Processing**: Handle keyboard, mouse, and scroll events
- **Math Utilities**: 3D vectors and matrices with cglm integration

#### Camera Controllers
- **FPS Camera** (`fps_camera.tl`): WASD movement with mouse look
- **Orbit Camera** (`orbit_camera.tl`): Rotate around target with zoom
- **Input Handler** (`input_handler.tl`): Centralized input management system

#### Usage Example
```lua
-- Create an FPS camera
local success = POC.camera_create_fps({x=0, y=2, z=5}, -90.0, 0.0, 16/9)
if success then
    POC.camera_set_fov(75.0)
    POC.camera_update(delta_time)
end
```

### Important Implementation Notes

- Uses C23 standard with compound literal initialization
- Platform detection via preprocessor defines (POC_PLATFORM_LINUX, POC_PLATFORM_MACOS)
- Error handling through `poc_result` enum with descriptive error codes
- Renderer abstraction allows runtime selection of graphics backend
- Context-based rendering model tied to Podi windows
- Application timing utilities: `poc_get_time()` returns elapsed seconds since init, `poc_sleep()` for delays

### API Structure

The engine follows a layered initialization pattern:
1. `poc_init()` with config - initializes graphics backend
2. `poc_context_create()` - creates rendering context for a Podi window
3. Frame rendering loop: `poc_context_begin_frame()` → rendering commands → `poc_context_end_frame()`
4. Cleanup: `poc_context_destroy()` → `poc_shutdown()`