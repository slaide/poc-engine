/**
 * @file poc_engine.h
 * @brief POC Engine - Cross-platform graphics rendering framework
 *
 * POC Engine provides a simple, cross-platform abstraction layer for 3D graphics
 * rendering. It currently supports Vulkan on Linux with planned Metal support for macOS.
 *
 * @section usage Basic Usage
 * @code
 * // 1. Initialize the engine
 * poc_config config = {
 *     .renderer_type = POC_RENDERER_VULKAN,
 *     .enable_validation = true,
 *     .app_name = "My App",
 *     .app_version = 1
 * };
 * poc_init(&config);
 *
 * // 2. Create a window and rendering context
 * podi_window *window = podi_window_create("My Window", 800, 600);
 * poc_context *ctx = poc_context_create(window);
 *
 * // 3. Render loop
 * while (!podi_window_should_close(window)) {
 *     podi_poll_events();
 *
 *     poc_context_begin_frame(ctx);
 *     poc_context_clear_color(ctx, 0.0f, 0.0f, 0.0f, 1.0f);
 *     // ... rendering commands ...
 *     poc_context_end_frame(ctx);
 * }
 *
 * // 4. Cleanup
 * poc_context_destroy(ctx);
 * podi_window_destroy(window);
 * poc_shutdown();
 * @endcode
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <cglm/cglm.h>
#include <podi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a rendering context
 *
 * A rendering context is tied to a specific window and manages all rendering
 * state for that window. Each window should have its own context.
 */
typedef struct poc_context poc_context;

/**
 * @brief Opaque handle to the graphics renderer backend
 *
 * This is an internal structure that manages the platform-specific graphics
 * implementation (Vulkan, Metal, etc.).
 */
typedef struct poc_renderer poc_renderer;

/**
 * @brief Opaque handle to a renderable object
 *
 * A renderable object encapsulates all data needed to render a 3D model,
 * including geometry, material properties, and transformation matrix.
 * Multiple renderables can exist within a single context.
 */
typedef struct poc_renderable poc_renderable;

/**
 * @brief Forward declarations for scene system
 */
typedef struct poc_scene poc_scene;
typedef struct poc_scene_object poc_scene_object;
typedef struct poc_mesh poc_mesh;
typedef struct poc_ray poc_ray;
typedef struct poc_hit_result poc_hit_result;

/**
 * @brief Graphics renderer backend types
 *
 * Specifies which graphics API backend to use for rendering.
 * The available backends depend on the target platform.
 */
typedef enum {
    POC_RENDERER_VULKAN,    /**< Vulkan renderer (Linux, Windows) */
#ifdef POC_PLATFORM_MACOS
    POC_RENDERER_METAL,     /**< Metal renderer (macOS only) */
#endif
} poc_renderer_type;

/**
 * @brief Result codes returned by POC Engine functions
 *
 * Most POC Engine functions return a result code to indicate success or failure.
 * Use poc_result_to_string() to get a human-readable description of the error.
 */
typedef enum {
    POC_RESULT_SUCCESS = 0,                         /**< Operation completed successfully */
    POC_RESULT_ERROR_INIT_FAILED,                   /**< Engine initialization failed */
    POC_RESULT_ERROR_DEVICE_NOT_FOUND,              /**< No suitable graphics device found */
    POC_RESULT_ERROR_SURFACE_CREATION_FAILED,       /**< Failed to create rendering surface */
    POC_RESULT_ERROR_SWAPCHAIN_CREATION_FAILED,     /**< Failed to create swapchain */
    POC_RESULT_ERROR_OUT_OF_MEMORY,                 /**< Out of memory */
    POC_RESULT_ERROR_SHADER_COMPILATION_FAILED,     /**< Shader compilation failed */
    POC_RESULT_ERROR_PIPELINE_CREATION_FAILED,      /**< Graphics pipeline creation failed */
} poc_result;

/**
 * @brief Configuration structure for engine initialization
 *
 * This structure contains all the settings needed to initialize the engine.
 * All fields must be set before passing to poc_init().
 */
typedef struct {
    poc_renderer_type renderer_type;    /**< Which graphics backend to use */
    bool enable_validation;             /**< Enable validation layers (debug builds) */
    const char *app_name;               /**< Application name (must not be NULL) */
    uint32_t app_version;               /**< Application version number */
} poc_config;

/**
 * @brief Initialize the POC Engine
 *
 * This must be called before any other POC Engine functions.
 * Initializes the graphics backend and sets up the engine state.
 *
 * @param config Configuration structure containing engine settings.
 *               Must not be NULL and all fields must be valid.
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note Only one engine instance can be active at a time.
 * @warning Must call poc_shutdown() before program exit to clean up resources.
 */
poc_result poc_init(const poc_config *config);

/**
 * @brief Shut down the POC Engine
 *
 * Destroys all engine resources and shuts down the graphics backend.
 * Must be called after destroying all contexts but before program exit.
 *
 * @warning All contexts must be destroyed before calling this function.
 * @note After calling this, poc_init() must be called again to use the engine.
 */
void poc_shutdown(void);

/**
 * @brief Create a rendering context for a window
 *
 * Creates a new rendering context tied to the specified window.
 * Each window should have its own context for rendering.
 *
 * @param window The Podi window to create a context for. Must not be NULL
 *               and must be a valid, open window.
 * @return Pointer to the new context on success, or NULL on failure
 *
 * @note The window must remain valid for the lifetime of the context.
 * @warning Must call poc_context_destroy() to free the context when done.
 */
poc_context *poc_context_create(podi_window *window);

/**
 * @brief Destroy a rendering context
 *
 * Destroys the specified rendering context and frees all associated resources.
 *
 * @param ctx The context to destroy. Can be NULL (no-op).
 *
 * @note After calling this, the context pointer becomes invalid.
 */
void poc_context_destroy(poc_context *ctx);

/**
 * @brief Begin a new frame for rendering
 *
 * Starts a new frame and prepares the context for rendering commands.
 * Must be called before any rendering operations and paired with
 * poc_context_end_frame().
 *
 * @param ctx The rendering context. Must not be NULL.
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note Must be paired with poc_context_end_frame() to complete the frame.
 * @warning Do not call this twice without calling poc_context_end_frame().
 */
poc_result poc_context_begin_frame(poc_context *ctx);

/**
 * @brief End the current frame and present it
 *
 * Completes the current frame and presents it to the screen.
 * Must be called after poc_context_begin_frame() and all rendering commands.
 *
 * @param ctx The rendering context. Must not be NULL.
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note Must be called after poc_context_begin_frame().
 */
poc_result poc_context_end_frame(poc_context *ctx);

/**
 * @brief Set the clear color for the next frame
 *
 * Sets the background color that will be used when clearing the screen
 * during the next frame. The color persists until changed.
 *
 * @param ctx The rendering context. Must not be NULL.
 * @param r Red component (0.0 to 1.0)
 * @param g Green component (0.0 to 1.0)
 * @param b Blue component (0.0 to 1.0)
 * @param a Alpha component (0.0 to 1.0)
 *
 * @note Color values are clamped to the range [0.0, 1.0].
 * @note Must be called between poc_context_begin_frame() and poc_context_end_frame().
 */
void poc_context_clear_color(poc_context *ctx, float r, float g, float b, float a);

/**
 * @brief Load and render a 3D model from an OBJ file (DEPRECATED)
 *
 * Loads a 3D model from the specified OBJ file and sets it up for rendering.
 * Supports materials (MTL files), vertex normals, texture coordinates, and
 * smoothing groups.
 *
 * @param ctx The rendering context. Must not be NULL.
 * @param obj_filename Path to the OBJ file to load. Must not be NULL.
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note The OBJ file should be in the same directory as any referenced MTL files.
 * @note Currently only one model can be loaded per context.
 * @warning File must be accessible and in valid OBJ format.
 * @deprecated Use poc_context_create_renderable() and poc_renderable_load_model() instead.
 */
poc_result poc_context_load_model(poc_context *ctx, const char *obj_filename);

/**
 * @brief Create a new renderable object
 *
 * Creates a new renderable object that can hold geometry, material properties,
 * and transformation data. The object is owned by the context and will be
 * automatically destroyed when the context is destroyed.
 *
 * @param ctx The rendering context. Must not be NULL.
 * @param name Optional name for the renderable (for debugging). Can be NULL.
 * @return Pointer to the new renderable on success, or NULL on failure
 *
 * @note The renderable is initially empty and must be loaded with a model.
 * @warning The renderable becomes invalid when the context is destroyed.
 */
poc_renderable *poc_context_create_renderable(poc_context *ctx, const char *name);

/**
 * @brief Destroy a renderable object
 *
 * Destroys the specified renderable object and frees all associated GPU resources.
 * The renderable is removed from the context's render list.
 *
 * @param ctx The rendering context that owns the renderable. Must not be NULL.
 * @param renderable The renderable to destroy. Can be NULL (no-op).
 *
 * @note After calling this, the renderable pointer becomes invalid.
 * @note This is called automatically when the context is destroyed.
 */
void poc_context_destroy_renderable(poc_context *ctx, poc_renderable *renderable);

/**
 * @brief Load a 3D model into a renderable object
 *
 * Loads geometry and material data from an OBJ file into the specified
 * renderable object. Any existing data in the renderable is replaced.
 *
 * @param renderable The renderable object to load into. Must not be NULL.
 * @param obj_filename Path to the OBJ file to load. Must not be NULL.
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note The OBJ file should be in the same directory as any referenced MTL files.
 * @warning File must be accessible and in valid OBJ format.
 */
poc_result poc_renderable_load_model(poc_renderable *renderable, const char *obj_filename);

/**
 * @brief Load mesh data into a renderable object
 *
 * Loads vertex and index data from a poc_mesh into the specified
 * renderable object. Any existing data in the renderable is replaced.
 *
 * @param renderable The renderable object to load into. Must not be NULL.
 * @param mesh The mesh containing vertex and index data. Must not be NULL.
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note This allows loading mesh data that's already in memory, unlike
 *       poc_renderable_load_model which loads from a file.
 */
poc_result poc_renderable_load_mesh(poc_renderable *renderable, poc_mesh *mesh);

/**
 * @brief Set the transformation matrix for a renderable
 *
 * Sets the model transformation matrix that will be applied when rendering
 * this object. This controls the object's position, rotation, and scale.
 *
 * @param renderable The renderable object. Must not be NULL.
 * @param transform 4x4 transformation matrix in column-major order
 *
 * @note The default transform is the identity matrix (no transformation).
 * @note Uses cglm's mat4 format (column-major, compatible with OpenGL/Vulkan).
 */
void poc_renderable_set_transform(poc_renderable *renderable, mat4 transform);

/**
 * @brief Get a human-readable string for a result code
 *
 * Converts a poc_result error code into a descriptive string for logging
 * or error reporting.
 *
 * @param result The result code to convert
 * @return Pointer to a static string describing the result.
 *         Never returns NULL.
 *
 * @note The returned string is static and should not be freed.
 */
const char *poc_result_to_string(poc_result result);

/**
 * @brief Get elapsed time since engine initialization
 *
 * Returns the number of seconds that have elapsed since poc_init() was called.
 * Useful for animations and timing calculations.
 *
 * @return Elapsed time in seconds as a double-precision floating point value
 *
 * @note Resolution depends on the system clock, typically nanosecond precision.
 * @warning Only valid after poc_init() has been called.
 */
double poc_get_time(void);

/**
 * @brief Sleep for the specified duration
 *
 * Suspends execution of the current thread for the specified number of seconds.
 * Useful for frame rate limiting or simple delays.
 *
 * @param seconds Number of seconds to sleep (can be fractional)
 *
 * @note Actual sleep time may be slightly longer due to system scheduling.
 * @note For frame rate limiting, consider using vertical sync instead.
 */
void poc_sleep(double seconds);

// ============================================================================
// Scene Management System
// ============================================================================

/**
 * @brief Create a new empty scene
 *
 * Creates a scene that can contain multiple scene objects for rendering
 * and object picking functionality.
 *
 * @return Pointer to new scene, or NULL on failure
 */
poc_scene* poc_scene_create(void);

/**
 * @brief Destroy a scene and optionally destroy its objects
 *
 * @param scene The scene to destroy
 * @param destroy_objects Whether to destroy the objects in the scene
 */
void poc_scene_destroy(poc_scene *scene, bool destroy_objects);

/**
 * @brief Create a new scene object
 *
 * @param name Human-readable name for the object
 * @param id Unique ID for the object (use poc_scene_get_next_object_id)
 * @return Pointer to new scene object, or NULL on failure
 */
poc_scene_object* poc_scene_object_create(const char *name, uint32_t id);

/**
 * @brief Destroy a scene object and free its resources
 *
 * @param obj The scene object to destroy
 */
void poc_scene_object_destroy(poc_scene_object *obj);

/**
 * @brief Load a mesh from an OBJ file
 *
 * @param filename Path to the OBJ file
 * @return Pointer to loaded mesh, or NULL on failure
 */
poc_mesh* poc_mesh_load(const char *filename);

/**
 * @brief Destroy a mesh and free its resources
 *
 * @param mesh The mesh to destroy
 */
void poc_mesh_destroy(poc_mesh *mesh);

/**
 * @brief Set the mesh component of a scene object
 *
 * @param obj The scene object
 * @param mesh The mesh to attach (can be NULL to remove)
 */
void poc_scene_object_set_mesh(poc_scene_object *obj, poc_mesh *mesh);

/**
 * @brief Set the position of a scene object
 *
 * @param obj The scene object
 * @param position New position
 */
void poc_scene_object_set_position(poc_scene_object *obj, vec3 position);

/**
 * @brief Set the rotation of a scene object
 *
 * @param obj The scene object
 * @param rotation New rotation in degrees (Euler angles)
 */
void poc_scene_object_set_rotation(poc_scene_object *obj, vec3 rotation);

/**
 * @brief Set the scale of a scene object
 *
 * @param obj The scene object
 * @param scale New scale factors
 */
void poc_scene_object_set_scale(poc_scene_object *obj, vec3 scale);

/**
 * @brief Get the current world transform matrix
 *
 * @param obj The scene object
 * @return Pointer to the 4x4 transform matrix
 */
const mat4* poc_scene_object_get_transform_matrix(poc_scene_object *obj);

/**
 * @brief Add an object to the scene
 *
 * @param scene The scene
 * @param object The object to add
 * @return True if added successfully, false otherwise
 */
bool poc_scene_add_object(poc_scene *scene, poc_scene_object *object);

/**
 * @brief Remove an object from the scene
 *
 * @param scene The scene
 * @param object The object to remove
 * @return True if removed successfully, false if not found
 */
bool poc_scene_remove_object(poc_scene *scene, poc_scene_object *object);

/**
 * @brief Find an object in the scene by ID
 *
 * @param scene The scene
 * @param id The object ID to search for
 * @return Pointer to object, or NULL if not found
 */
poc_scene_object* poc_scene_find_object_by_id(poc_scene *scene, uint32_t id);

/**
 * @brief Get the next available object ID
 *
 * @param scene The scene
 * @return Next available object ID
 */
uint32_t poc_scene_get_next_object_id(poc_scene *scene);

/**
 * @brief Update all objects in the scene
 *
 * Updates transforms and bounds for all dirty objects.
 *
 * @param scene The scene
 */
void poc_scene_update(poc_scene *scene);

/**
 * @brief Perform picking ray cast against all objects in the scene
 *
 * Tests the ray against all renderable objects and returns the closest hit.
 *
 * @param scene The scene
 * @param ray The picking ray
 * @param hit_result Output hit result (closest hit or no hit)
 * @return True if any object was hit, false otherwise
 */
bool poc_scene_pick_object(poc_scene *scene,
                          const poc_ray *ray,
                          poc_hit_result *hit_result);

/**
 * @brief Set the active scene for a rendering context
 *
 * The active scene's objects will be automatically rendered during
 * begin_frame/end_frame calls. This replaces the need to manually
 * call poc_context_render_scene.
 *
 * @param ctx The rendering context. Must not be NULL.
 * @param scene The scene to make active, or NULL to clear the active scene
 */
void poc_context_set_scene(poc_context *ctx, poc_scene *scene);

/**
 * @brief Render all objects in a scene using the specified context
 *
 * @param ctx The rendering context
 * @param scene The scene containing objects to render
 * @return POC_RESULT_SUCCESS on success, or an error code on failure
 */
poc_result poc_context_render_scene(poc_context *ctx, poc_scene *scene);

/**
 * @brief Toggle play mode (lit) rendering on the context
 *
 * When play mode is disabled the renderer outputs unlit diffuse colors,
 * useful while editing. Enabling play mode restores full lighting.
 *
 * @param ctx Rendering context to update. Must not be NULL.
 * @param enabled True for play mode (lit), false for edit mode (unlit)
 */
void poc_context_set_play_mode(poc_context *ctx, bool enabled);

/**
 * @brief Query the current play/edit mode state
 *
 * @param ctx Rendering context to inspect
 * @return True if play mode (lit) is active, otherwise false
 */
bool poc_context_is_play_mode(poc_context *ctx);

#ifdef __cplusplus
}
#endif
