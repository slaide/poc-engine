/**
 * @file camera.h
 * @brief 3D camera system for POC Engine
 *
 * This module provides a flexible camera system supporting multiple camera types
 * including first-person, orbit, and free cameras. The camera system is fully
 * scriptable via Lua and integrates with the engine's input system.
 */

#pragma once

#include <cglm/cglm.h>
#include <stdbool.h>
#include <podi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera types supported by the system
 */
typedef enum {
    POC_CAMERA_FIRST_PERSON,    /**< First-person camera (FPS-style) */
    POC_CAMERA_ORBIT,           /**< Orbit camera around a target point */
    POC_CAMERA_FREE,            /**< Free-flying camera */
} poc_camera_type;

/**
 * @brief 3D camera structure
 *
 * Represents a 3D camera with position, orientation, and projection parameters.
 * Supports multiple camera types and provides view/projection matrix generation.
 */
typedef struct {
    // Camera type and mode
    poc_camera_type type;
    bool enabled;

    // Position and orientation
    vec3 position;           /**< Camera position in world space */
    vec3 front;              /**< Forward direction vector */
    vec3 up;                 /**< Up direction vector */
    vec3 right;              /**< Right direction vector */
    vec3 world_up;           /**< World up vector (usually 0, 1, 0) */

    // Euler angles (in degrees)
    float yaw;               /**< Rotation around Y axis */
    float pitch;             /**< Rotation around X axis */
    float roll;              /**< Rotation around Z axis (rarely used) */

    // Projection parameters
    float fov;               /**< Field of view in degrees */
    float aspect_ratio;      /**< Aspect ratio (width/height) */
    float near_plane;        /**< Near clipping plane distance */
    float far_plane;         /**< Far clipping plane distance */

    // Camera-specific parameters
    union {
        struct {
            float movement_speed;    /**< Movement speed in units/second */
            float mouse_sensitivity; /**< Mouse sensitivity for looking */
            bool constrain_pitch;    /**< Limit pitch to [-89, 89] degrees */
        } fps;

        struct {
            vec3 target;             /**< Point to orbit around */
            float distance;          /**< Distance from target */
            float zoom_speed;        /**< Zoom speed multiplier */
            float orbit_speed;       /**< Orbit rotation speed */
            float min_distance;      /**< Minimum orbit distance */
            float max_distance;      /**< Maximum orbit distance */
        } orbit;

        struct {
            float movement_speed;    /**< Movement speed in units/second */
            float rotation_speed;    /**< Rotation speed in degrees/second */
        } free;
    };

    // Computed matrices (updated automatically)
    mat4 view_matrix;        /**< View transformation matrix */
    mat4 projection_matrix;  /**< Projection transformation matrix */
    bool matrices_dirty;     /**< Whether matrices need recalculation */

    // Input state (for internal use)
    struct {
        bool keys_pressed[64];  // Enough for all podi keys
        double last_mouse_x, last_mouse_y;
        bool first_mouse;
        double delta_time;
    } input;
} poc_camera;

/**
 * @brief Create a new camera with the specified type
 *
 * Initializes a camera with default parameters for the given type.
 * The camera is created at the origin looking down the negative Z axis.
 *
 * @param type The type of camera to create
 * @param aspect_ratio Initial aspect ratio (width/height)
 * @return Initialized camera structure
 */
poc_camera poc_camera_create(poc_camera_type type, float aspect_ratio);

/**
 * @brief Create a first-person camera with custom parameters
 *
 * Creates an FPS-style camera with the specified position and orientation.
 *
 * @param position Initial camera position
 * @param yaw Initial yaw angle in degrees
 * @param pitch Initial pitch angle in degrees
 * @param aspect_ratio Aspect ratio (width/height)
 * @return Initialized first-person camera
 */
poc_camera poc_camera_create_fps(vec3 position, float yaw, float pitch, float aspect_ratio);

/**
 * @brief Create an orbit camera with custom parameters
 *
 * Creates a camera that orbits around a target point at a fixed distance.
 *
 * @param target Point to orbit around
 * @param distance Initial distance from target
 * @param yaw Initial orbital yaw angle in degrees
 * @param pitch Initial orbital pitch angle in degrees
 * @param aspect_ratio Aspect ratio (width/height)
 * @return Initialized orbit camera
 */
poc_camera poc_camera_create_orbit(vec3 target, float distance, float yaw, float pitch, float aspect_ratio);

/**
 * @brief Update camera matrices
 *
 * Recalculates the view and projection matrices if they are dirty.
 * This is called automatically by other camera functions as needed.
 *
 * @param camera The camera to update. Must not be NULL.
 */
void poc_camera_update_matrices(poc_camera *camera);

/**
 * @brief Set camera position
 *
 * Sets the camera position and marks matrices as dirty for recalculation.
 *
 * @param camera The camera to modify. Must not be NULL.
 * @param position New camera position
 */
void poc_camera_set_position(poc_camera *camera, vec3 position);

/**
 * @brief Set camera orientation using Euler angles
 *
 * Sets the camera orientation and updates direction vectors.
 *
 * @param camera The camera to modify. Must not be NULL.
 * @param yaw Yaw angle in degrees
 * @param pitch Pitch angle in degrees
 * @param roll Roll angle in degrees (optional, usually 0)
 */
void poc_camera_set_rotation(poc_camera *camera, float yaw, float pitch, float roll);

/**
 * @brief Set camera field of view
 *
 * Sets the camera's field of view and marks projection matrix as dirty.
 *
 * @param camera The camera to modify. Must not be NULL.
 * @param fov Field of view in degrees (typically 45-90)
 */
void poc_camera_set_fov(poc_camera *camera, float fov);

/**
 * @brief Set camera aspect ratio
 *
 * Updates the camera's aspect ratio and marks projection matrix as dirty.
 * Typically called when the window is resized.
 *
 * @param camera The camera to modify. Must not be NULL.
 * @param aspect_ratio New aspect ratio (width/height)
 */
void poc_camera_set_aspect_ratio(poc_camera *camera, float aspect_ratio);

/**
 * @brief Get the camera's view matrix
 *
 * Returns the current view matrix, updating it if necessary.
 *
 * @param camera The camera to query. Must not be NULL.
 * @return Pointer to the view matrix (column-major, cglm format)
 */
const float *poc_camera_get_view_matrix(poc_camera *camera);

/**
 * @brief Get the camera's projection matrix
 *
 * Returns the current projection matrix, updating it if necessary.
 *
 * @param camera The camera to query. Must not be NULL.
 * @return Pointer to the projection matrix (column-major, cglm format)
 */
const float *poc_camera_get_projection_matrix(poc_camera *camera);

/**
 * @brief Process keyboard input for camera movement
 *
 * Updates camera position/orientation based on keyboard input.
 * This should be called each frame with the current input state.
 *
 * @param camera The camera to update. Must not be NULL.
 * @param key The key that was pressed/released
 * @param pressed Whether the key is currently pressed
 * @param delta_time Time elapsed since last frame in seconds
 */
void poc_camera_process_keyboard(poc_camera *camera, podi_key key, bool pressed, double delta_time);

/**
 * @brief Process mouse movement for camera orientation
 *
 * Updates camera orientation based on mouse movement.
 * This should be called when mouse movement events are received.
 *
 * @param camera The camera to update. Must not be NULL.
 * @param mouse_x Current mouse X position
 * @param mouse_y Current mouse Y position
 * @param constrain_pitch Whether to limit pitch rotation
 */
void poc_camera_process_mouse_movement(poc_camera *camera, double mouse_x, double mouse_y, bool constrain_pitch);

/**
 * @brief Process mouse scroll for camera zoom/FOV
 *
 * Updates camera zoom or field of view based on mouse scroll input.
 *
 * @param camera The camera to update. Must not be NULL.
 * @param scroll_y Vertical scroll amount
 */
void poc_camera_process_mouse_scroll(poc_camera *camera, double scroll_y);

/**
 * @brief Update camera state each frame
 *
 * Performs per-frame camera updates including movement, rotation,
 * and matrix recalculation. This should be called once per frame.
 *
 * @param camera The camera to update. Must not be NULL.
 * @param delta_time Time elapsed since last frame in seconds
 */
void poc_camera_update(poc_camera *camera, double delta_time);

/**
 * @brief Get camera's front direction vector
 *
 * Returns the normalized front direction vector of the camera.
 *
 * @param camera The camera to query. Must not be NULL.
 * @return Pointer to the front direction vector
 */
const float *poc_camera_get_front(poc_camera *camera);

/**
 * @brief Get camera's up direction vector
 *
 * Returns the normalized up direction vector of the camera.
 *
 * @param camera The camera to query. Must not be NULL.
 * @return Pointer to the up direction vector
 */
const float *poc_camera_get_up(poc_camera *camera);

/**
 * @brief Get camera's right direction vector
 *
 * Returns the normalized right direction vector of the camera.
 *
 * @param camera The camera to query. Must not be NULL.
 * @return Pointer to the right direction vector
 */
const float *poc_camera_get_right(poc_camera *camera);

#ifdef __cplusplus
}
#endif