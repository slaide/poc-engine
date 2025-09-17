#pragma once

#include "poc_engine.h"

#ifdef POC_PLATFORM_LINUX

poc_result vulkan_init(const poc_config *config);
void vulkan_shutdown(void);

poc_context *vulkan_context_create(podi_window *window);
void vulkan_context_destroy(poc_context *ctx);

poc_result vulkan_context_begin_frame(poc_context *ctx);
poc_result vulkan_context_end_frame(poc_context *ctx);

void vulkan_context_clear_color(poc_context *ctx, float r, float g, float b, float a);

#endif