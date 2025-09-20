/**
 * @file vulkan_renderer.h
 * @brief Vulkan graphics backend implementation
 *
 * This header contains the internal Vulkan renderer interface for POC Engine.
 * These functions implement the platform-specific graphics operations for Linux
 * and other platforms that support Vulkan.
 *
 * @warning This is an internal header and should not be used directly by applications.
 *          Use the functions in poc_engine.h instead, which provide a platform-agnostic
 *          interface that automatically delegates to the appropriate backend.
 *
 * @note This header is only available when POC_PLATFORM_LINUX is defined.
 */

#pragma once

#include "poc_engine.h"
#include "obj_loader.h"

#ifdef POC_PLATFORM_LINUX

/**
 * @brief Initialize the Vulkan graphics backend
 *
 * Internal function that sets up the Vulkan instance, device, and other
 * global Vulkan resources. Called by poc_init() when Vulkan renderer is selected.
 *
 * @param config Engine configuration (must specify POC_RENDERER_VULKAN)
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note This is an internal function - use poc_init() instead.
 */
poc_result vulkan_init(const poc_config *config);

/**
 * @brief Shut down the Vulkan graphics backend
 *
 * Internal function that destroys all global Vulkan resources.
 * Called by poc_shutdown() when cleaning up the engine.
 *
 * @note This is an internal function - use poc_shutdown() instead.
 */
void vulkan_shutdown(void);

/**
 * @brief Create a Vulkan rendering context for a window
 *
 * Internal function that creates the Vulkan surface, swapchain, and other
 * per-window resources needed for rendering.
 *
 * @param window The Podi window to create a context for
 * @return Pointer to the new context on success, or NULL on failure
 *
 * @note This is an internal function - use poc_context_create() instead.
 */
poc_context *vulkan_context_create(podi_window *window);

/**
 * @brief Destroy a Vulkan rendering context
 *
 * Internal function that destroys all Vulkan resources associated with
 * the specified context.
 *
 * @param ctx The context to destroy
 *
 * @note This is an internal function - use poc_context_destroy() instead.
 */
void vulkan_context_destroy(poc_context *ctx);

/**
 * @brief Begin a new Vulkan frame
 *
 * Internal function that acquires the next swapchain image and begins
 * command buffer recording for the frame.
 *
 * @param ctx The rendering context
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note This is an internal function - use poc_context_begin_frame() instead.
 */
poc_result vulkan_context_begin_frame(poc_context *ctx);

/**
 * @brief End the current Vulkan frame
 *
 * Internal function that ends command buffer recording, submits the
 * command buffer to the graphics queue, and presents the rendered image.
 *
 * @param ctx The rendering context
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note This is an internal function - use poc_context_end_frame() instead.
 */
poc_result vulkan_context_end_frame(poc_context *ctx);

/**
 * @brief Set the Vulkan clear color
 *
 * Internal function that sets the clear color for the Vulkan render pass.
 *
 * @param ctx The rendering context
 * @param r Red component (0.0 to 1.0)
 * @param g Green component (0.0 to 1.0)
 * @param b Blue component (0.0 to 1.0)
 * @param a Alpha component (0.0 to 1.0)
 *
 * @note This is an internal function - use poc_context_clear_color() instead.
 */
void vulkan_context_clear_color(poc_context *ctx, float r, float g, float b, float a);

/**
 * @brief Load a 3D model for Vulkan rendering
 *
 * Internal function that loads an OBJ file and creates the necessary
 * Vulkan vertex and index buffers for rendering.
 *
 * @param ctx The rendering context
 * @param obj_filename Path to the OBJ file to load
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note This is an internal function - use poc_context_load_model() instead.
 */
poc_result vulkan_context_load_model(poc_context *ctx, const char *obj_filename);

/**
 * @brief Set vertex data for Vulkan rendering
 *
 * Internal function that creates Vulkan vertex and index buffers from
 * the provided vertex and index data.
 *
 * @param ctx The rendering context
 * @param vertices Array of vertex data
 * @param vertex_count Number of vertices in the array
 * @param indices Array of index data
 * @param index_count Number of indices in the array
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note This is an internal function used by vulkan_context_load_model().
 */
poc_result vulkan_context_set_vertex_data(poc_context *ctx, poc_vertex *vertices, uint32_t vertex_count, uint32_t *indices, uint32_t index_count);

#endif