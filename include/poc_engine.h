#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <podi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct poc_context poc_context;
typedef struct poc_renderer poc_renderer;

typedef enum {
    POC_RENDERER_VULKAN,
#ifdef POC_PLATFORM_MACOS
    POC_RENDERER_METAL,
#endif
} poc_renderer_type;

typedef enum {
    POC_RESULT_SUCCESS = 0,
    POC_RESULT_ERROR_INIT_FAILED,
    POC_RESULT_ERROR_DEVICE_NOT_FOUND,
    POC_RESULT_ERROR_SURFACE_CREATION_FAILED,
    POC_RESULT_ERROR_SWAPCHAIN_CREATION_FAILED,
    POC_RESULT_ERROR_OUT_OF_MEMORY,
    POC_RESULT_ERROR_SHADER_COMPILATION_FAILED,
    POC_RESULT_ERROR_PIPELINE_CREATION_FAILED,
} poc_result;

typedef struct {
    poc_renderer_type renderer_type;
    bool enable_validation;
    const char *app_name;
    uint32_t app_version;
} poc_config;

poc_result poc_init(const poc_config *config);
void poc_shutdown(void);

poc_context *poc_context_create(podi_window *window);
void poc_context_destroy(poc_context *ctx);

poc_result poc_context_begin_frame(poc_context *ctx);
poc_result poc_context_end_frame(poc_context *ctx);

void poc_context_clear_color(poc_context *ctx, float r, float g, float b, float a);

// Model loading and rendering
poc_result poc_context_load_model(poc_context *ctx, const char *obj_filename);

const char *poc_result_to_string(poc_result result);

double poc_get_time(void);
void poc_sleep(double seconds);

#ifdef __cplusplus
}
#endif