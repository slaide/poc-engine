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
- **Platform Libraries**:
  - Linux: Vulkan, X11, Wayland, xkbcommon
  - macOS: Cocoa, Metal, MetalKit frameworks

### Build System

The Makefile automatically:
- Detects platform and architecture
- Sets appropriate compiler flags and platform defines
- Clones and builds Podi dependency if needed
- Links examples against the engine and platform libraries

### Development Workflow

1. Place new examples in `examples/` directory
2. Engine source goes in `src/`
3. Public headers in `include/`
4. GLSL shaders go in `shaders/` directory (compiled automatically)
5. Use `make run` for quick testing
6. Use `PODI_LOCAL_DIR=/path/to/podi make` for local Podi development

### Shader Development

- Place GLSL shaders in `shaders/` directory with `.vert` and `.frag` extensions
- Shaders are automatically compiled to SPIR-V during build using glslangValidator
- Current implementation includes a triangle shader that renders a colored triangle
- Shaders are loaded at runtime from the compiled `.spv` files

### Important Implementation Notes

- Uses C23 standard with compound literal initialization
- Platform detection via preprocessor defines (POC_PLATFORM_LINUX, POC_PLATFORM_MACOS)
- Error handling through `poc_result` enum with descriptive error codes
- Renderer abstraction allows runtime selection of graphics backend
- Context-based rendering model tied to Podi windows