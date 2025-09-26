#define _POSIX_C_SOURCE 199309L
#include "poc_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef POC_PLATFORM_LINUX
#include "vulkan_renderer.h"
#endif

#ifdef POC_PLATFORM_MACOS
#include "metal_renderer.h"
#endif

static poc_renderer_type g_current_renderer = POC_RENDERER_VULKAN;
static bool g_initialized = false;
static struct timespec g_start_time = {0};

poc_result poc_init(const poc_config *config) {
    if (g_initialized) {
        return POC_RESULT_SUCCESS;
    }

    if (!config) {
        printf("poc_init: config cannot be NULL\n");
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    g_current_renderer = config->renderer_type;

#ifdef POC_PLATFORM_LINUX
    if (config->renderer_type == POC_RENDERER_VULKAN) {
        poc_result result = vulkan_init(config);
        if (result != POC_RESULT_SUCCESS) {
            printf("Failed to initialize Vulkan renderer: %s\n", poc_result_to_string(result));
            return result;
        }
    } else {
        printf("poc_init: Unsupported renderer type for Linux: %d\n", config->renderer_type);
        return POC_RESULT_ERROR_INIT_FAILED;
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (config->renderer_type == POC_RENDERER_METAL) {
        poc_result result = metal_init(config);
        if (result != POC_RESULT_SUCCESS) {
            printf("Failed to initialize Metal renderer: %s\n", poc_result_to_string(result));
            return result;
        }
    } else if (config->renderer_type == POC_RENDERER_VULKAN) {
        printf("poc_init: Vulkan on macOS not yet supported, use Metal\n");
        return POC_RESULT_ERROR_INIT_FAILED;
    } else {
        printf("poc_init: Unsupported renderer type for macOS: %d\n", config->renderer_type);
        return POC_RESULT_ERROR_INIT_FAILED;
    }
#endif

    // Initialize the application start time
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);

    g_initialized = true;
    printf("POC Engine initialized with %s renderer\n",
           config->renderer_type == POC_RENDERER_VULKAN ? "Vulkan" : "Metal");

    return POC_RESULT_SUCCESS;
}

void poc_shutdown(void) {
    if (!g_initialized) {
        return;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        vulkan_shutdown();
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        metal_shutdown();
    }
#endif

    g_initialized = false;
    printf("POC Engine shut down\n");
}

poc_context *poc_context_create(podi_window *window) {
    if (!g_initialized) {
        printf("poc_context_create: Engine not initialized\n");
        return NULL;
    }

    if (!window) {
        printf("poc_context_create: window cannot be NULL\n");
        return NULL;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        return vulkan_context_create(window);
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        return metal_context_create(window);
    }
#endif

    printf("poc_context_create: No renderer available\n");
    return NULL;
}

void poc_context_destroy(poc_context *ctx) {
    if (!ctx) {
        return;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        vulkan_context_destroy(ctx);
        return;
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        metal_context_destroy(ctx);
        return;
    }
#endif
}

poc_result poc_context_begin_frame(poc_context *ctx) {
    if (!ctx) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        return vulkan_context_begin_frame(ctx);
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        return metal_context_begin_frame(ctx);
    }
#endif

    return POC_RESULT_ERROR_INIT_FAILED;
}

poc_result poc_context_end_frame(poc_context *ctx) {
    if (!ctx) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        return vulkan_context_end_frame(ctx);
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        return metal_context_end_frame(ctx);
    }
#endif

    return POC_RESULT_ERROR_INIT_FAILED;
}

void poc_context_clear_color(poc_context *ctx, float r, float g, float b, float a) {
    if (!ctx) {
        return;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        vulkan_context_clear_color(ctx, r, g, b, a);
        return;
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        metal_context_clear_color(ctx, r, g, b, a);
        return;
    }
#endif
}

poc_result poc_context_load_model(poc_context *ctx, const char *obj_filename) {
    if (!ctx || !obj_filename) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        return vulkan_context_load_model(ctx, obj_filename);
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        // Metal implementation would go here
        printf("Model loading not yet implemented for Metal renderer\n");
        return POC_RESULT_ERROR_INIT_FAILED;
    }
#endif

    return POC_RESULT_ERROR_INIT_FAILED;
}

const char *poc_result_to_string(poc_result result) {
    switch (result) {
        case POC_RESULT_SUCCESS: return "Success";
        case POC_RESULT_ERROR_INIT_FAILED: return "Initialization failed";
        case POC_RESULT_ERROR_DEVICE_NOT_FOUND: return "Graphics device not found";
        case POC_RESULT_ERROR_SURFACE_CREATION_FAILED: return "Surface creation failed";
        case POC_RESULT_ERROR_SWAPCHAIN_CREATION_FAILED: return "Swapchain creation failed";
        case POC_RESULT_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case POC_RESULT_ERROR_SHADER_COMPILATION_FAILED: return "Shader compilation failed";
        case POC_RESULT_ERROR_PIPELINE_CREATION_FAILED: return "Pipeline creation failed";
        default: return "Unknown error";
    }
}

double poc_get_time(void) {
    if (!g_initialized) {
        return 0.0;
    }

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    // Calculate elapsed time since application start
    double elapsed_seconds = (double)(current_time.tv_sec - g_start_time.tv_sec);
    elapsed_seconds += (double)(current_time.tv_nsec - g_start_time.tv_nsec) / 1000000000.0;

    return elapsed_seconds;
}

void poc_sleep(double seconds) {
    if (seconds <= 0.0) {
        return;
    }

    struct timespec sleep_time = {
        .tv_sec = (time_t)seconds,
        .tv_nsec = (long)((seconds - (time_t)seconds) * 1000000000.0)
    };

    nanosleep(&sleep_time, NULL);
}

void poc_context_set_scene(poc_context *ctx, poc_scene *scene) {
    if (!ctx) {
        return;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        vulkan_context_set_scene(ctx, scene);
        return;
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        metal_context_set_scene(ctx, scene);
        return;
    }
#endif
}

poc_result poc_context_render_scene(poc_context *ctx, poc_scene *scene) {
    if (!ctx || !scene) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

#ifdef POC_PLATFORM_LINUX
    if (g_current_renderer == POC_RENDERER_VULKAN) {
        return vulkan_context_render_scene(ctx, scene);
    }
#endif

#ifdef POC_PLATFORM_MACOS
    if (g_current_renderer == POC_RENDERER_METAL) {
        return metal_context_render_scene(ctx, scene);
    }
#endif

    return POC_RESULT_ERROR_INIT_FAILED;
}