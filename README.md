# POC Engine - Graphics Framework

POC Engine is a graphics framework that renders to windows provided by the [Podi](https://github.com/slaide/podi) cross-platform window management library.

## Features

- **Cross-platform graphics rendering**:
  - Vulkan on Linux
  - Metal on macOS (planned)
- **Window management via Podi**
- **C23 implementation**
- **Git submodule dependency management** (cglm math library, Podi windowing)

## Building

### Prerequisites

**Linux (x64/ARM64):**
- GCC with C23 support
- Vulkan development libraries (`libvulkan-dev` on Ubuntu/Debian)
- X11 development libraries (`libx11-dev` on Ubuntu/Debian)
- Wayland development libraries (`libwayland-dev wayland-protocols` on Ubuntu/Debian)
- GLSL shader compiler (`glslang-tools` on Ubuntu/Debian)

**macOS (ARM64):**
- Xcode command line tools
- Clang with C23 support

### Build Instructions

```bash
# Clone the repository with submodules
git clone --recursive <repository-url>
cd poc-engine

# Or if already cloned, initialize submodules
git submodule update --init --recursive

# Build everything (automatically updates submodules and builds dependencies)
make

# Build with debug symbols
make debug

# Build optimized release
make release

# Clean build artifacts
make clean

# Clean everything including dependencies
make clean-all
```

## Usage

### Basic Example

```c
#include "poc_engine.h"
#include <stdio.h>
#include <math.h>

int my_main(podi_application *app) {
    // Configure the graphics engine
    poc_config config = {
        .renderer_type = POC_RENDERER_VULKAN,  // Use Vulkan on Linux
        .enable_validation = true,              // Enable debug validation
        .app_name = "My App",
        .app_version = 1
    };

    // Initialize the engine
    if (poc_init(&config) != POC_RESULT_SUCCESS) {
        printf("Failed to initialize POC Engine\n");
        return -1;
    }

    // Create a window using Podi
    podi_window *window = podi_window_create(app, "My Window", 800, 600);
    if (!window) {
        printf("Failed to create window\n");
        poc_shutdown();
        return -1;
    }

    // Create a rendering context
    poc_context *ctx = poc_context_create(window);
    if (!ctx) {
        printf("Failed to create rendering context\n");
        podi_window_destroy(window);
        poc_shutdown();
        return -1;
    }

    // Main loop
    while (!podi_application_should_close(app) && !podi_window_should_close(window)) {
        // Handle events
        podi_event event;
        while (podi_application_poll_event(app, &event)) {
            switch (event.type) {
                case PODI_EVENT_WINDOW_CLOSE:
                    podi_window_close(window);
                    break;
                case PODI_EVENT_KEY_DOWN:
                    if (event.key.key == PODI_KEY_ESCAPE) {
                        podi_window_close(window);
                    }
                    break;
                default:
                    break;
            }
        }

        // Render frame
        if (poc_context_begin_frame(ctx) == POC_RESULT_SUCCESS) {
            poc_context_clear_color(ctx, 0.2f, 0.3f, 0.8f, 1.0f);
            poc_context_end_frame(ctx);
        }
    }

    // Cleanup
    poc_context_destroy(ctx);
    podi_window_destroy(window);
    poc_shutdown();

    return 0;
}

int main(void) {
    return podi_main(my_main);
}
```

### Running the Example

The easiest way to run the basic example:

```bash
# Option 1: Using make
make run

# Option 2: Using the run script
./run.sh

# Option 3: Manual execution (with proper library paths)
LD_LIBRARY_PATH=deps/podi/lib:$LD_LIBRARY_PATH ./examples/basic
```

### Compiling Your Application

```bash
# Linux (assuming poc-engine is in your system)
gcc -std=c23 your_app.c -lpoc_engine -lpodi -lX11 -lwayland-client -lvulkan -ldl -lm -o your_app

# Or build within the poc-engine directory structure
# Place your source in examples/ and run make
```

## API Reference

### Initialization

- `poc_result poc_init(const poc_config *config)` - Initialize the graphics engine
- `void poc_shutdown(void)` - Shutdown the graphics engine

### Context Management

- `poc_context *poc_context_create(podi_window *window)` - Create rendering context
- `void poc_context_destroy(poc_context *ctx)` - Destroy rendering context

### Rendering

- `poc_result poc_context_begin_frame(poc_context *ctx)` - Begin frame rendering
- `poc_result poc_context_end_frame(poc_context *ctx)` - End frame rendering
- `void poc_context_clear_color(poc_context *ctx, float r, float g, float b, float a)` - Set clear color

### Configuration

```c
typedef struct {
    poc_renderer_type renderer_type;  // POC_RENDERER_VULKAN or POC_RENDERER_METAL
    bool enable_validation;           // Enable debug validation layers
    const char *app_name;            // Application name
    uint32_t app_version;            // Application version
} poc_config;
```

## Project Structure

```
poc-engine/
├── src/                     # Engine source code
│   ├── poc_engine.c        # Main engine implementation
│   ├── vulkan_renderer.c   # Vulkan backend (Linux)
│   └── vulkan_renderer.h   # Vulkan backend header
├── include/                # Public headers
│   └── poc_engine.h       # Main engine header
├── examples/              # Example applications
│   └── basic.c           # Basic example
├── deps/                 # Dependencies (auto-generated)
│   └── podi/            # Podi library (cloned automatically)
├── obj/                 # Build artifacts (auto-generated)
├── Makefile            # Build system
└── README.md          # This file
```

## Architecture

POC Engine is designed as a lightweight graphics abstraction layer on top of Podi's window management:

```
┌─────────────────┐
│   Your App      │
├─────────────────┤
│   POC Engine    │  (Graphics abstraction)
├─────────────────┤
│      Podi       │  (Window management)
├─────────────────┤
│ Vulkan | Metal  │  (Graphics APIs)
└─────────────────┘
```

## Status

- ✅ Linux Vulkan backend with complete 3D rendering pipeline
- ✅ Git submodule dependency management (cglm, Podi)
- ✅ 3D cube rendering with perspective camera
- ✅ GLSL shaders compiled to SPIR-V
- ✅ Uniform buffer objects and descriptor sets
- ✅ Proper backface culling and matrix transformations
- ✅ Window resizing and swapchain recreation
- 🚧 macOS Metal backend
- 🚧 Advanced rendering features (textures, lighting, models)

## Contributing

This is an early-stage project. Contributions are welcome! The focus is on:

1. Completing the Vulkan rendering pipeline
2. Adding macOS Metal support
3. Adding more rendering features
4. Performance optimizations

## License

[License information needed]