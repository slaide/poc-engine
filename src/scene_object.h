/**
 * @file scene_object.h
 * @brief Scene object system for POC Engine
 *
 * This module provides a scene graph system with transforms, components,
 * and world-space bounding information for objects.
 */

#pragma once

#include "mesh.h"
#include <cglm/cglm.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct poc_renderable poc_renderable;

/**
 * @brief Scene object representing an entity in the 3D world
 *
 * Contains transform, components (mesh, material), and computed world-space bounds.
 * Forms the basis of the scene graph system.
 */
typedef struct poc_scene_object {
    // Identification
    uint32_t id;                /**< Unique object ID */
    char name[256];             /**< Human-readable name */

    // Transform components
    vec3 position;              /**< Local position */
    vec3 rotation;              /**< Euler angles in degrees */
    vec3 scale;                 /**< Scale factors */
    mat4 transform_matrix;      /**< Computed world transform matrix */
    bool transform_dirty;       /**< Whether transform needs recalculation */

    // Components
    poc_mesh *mesh;             /**< Mesh component (optional) */
    poc_material *material;     /**< Material component (optional) */
    poc_renderable *renderable; /**< Associated renderable for rendering (optional) */

    // Computed world-space bounds (from mesh + transform)
    vec3 world_aabb_min;        /**< World-space AABB minimum */
    vec3 world_aabb_max;        /**< World-space AABB maximum */
    bool bounds_dirty;          /**< Whether bounds need recalculation */

    // Scene graph (for future expansion)
    struct poc_scene_object *parent;       /**< Parent object */
    struct poc_scene_object **children;    /**< Array of child objects */
    uint32_t child_count;                  /**< Number of children */
    uint32_t child_capacity;               /**< Capacity of children array */

    // State
    bool visible;               /**< Whether object should be rendered */
    bool enabled;               /**< Whether object is active in scene */
} poc_scene_object;

/**
 * @brief Create a new scene object
 *
 * Creates a scene object with default transform (identity) and no components.
 *
 * @param name Human-readable name for the object
 * @param id Unique ID for the object
 * @return Pointer to new scene object, or NULL on failure
 */
poc_scene_object* poc_scene_object_create(const char *name, uint32_t id);

/**
 * @brief Destroy a scene object and free its resources
 *
 * Destroys the scene object but does not destroy referenced components
 * (mesh, material) as they may be shared.
 *
 * @param obj The scene object to destroy
 */
void poc_scene_object_destroy(poc_scene_object *obj);

/**
 * @brief Set the mesh component of a scene object
 *
 * The object will reference the mesh but not take ownership.
 * Updates bounds when mesh changes.
 *
 * @param obj The scene object
 * @param mesh The mesh to attach (can be NULL to remove)
 */
void poc_scene_object_set_mesh(poc_scene_object *obj, poc_mesh *mesh);

/**
 * @brief Set the material component of a scene object
 *
 * The object will reference the material but not take ownership.
 *
 * @param obj The scene object
 * @param material The material to attach (can be NULL to remove)
 */
void poc_scene_object_set_material(poc_scene_object *obj, poc_material *material);

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
 * @brief Set the transform using individual components
 *
 * @param obj The scene object
 * @param position Position vector
 * @param rotation Rotation in degrees (Euler angles)
 * @param scale Scale factors
 */
void poc_scene_object_set_transform(poc_scene_object *obj,
                                    vec3 position,
                                    vec3 rotation,
                                    vec3 scale);

/**
 * @brief Update the transform matrix from position/rotation/scale
 *
 * Recalculates the transform matrix if it's dirty.
 * Also updates world-space bounds if needed.
 *
 * @param obj The scene object
 */
void poc_scene_object_update_transform(poc_scene_object *obj);

/**
 * @brief Get the current world transform matrix
 *
 * Updates the transform if dirty before returning it.
 *
 * @param obj The scene object
 * @return Pointer to the 4x4 transform matrix
 */
const mat4* poc_scene_object_get_transform_matrix(poc_scene_object *obj);

/**
 * @brief Update world-space bounding box from mesh and transform
 *
 * Recalculates world AABB by transforming the local mesh bounds.
 * Called automatically when transform or mesh changes.
 *
 * @param obj The scene object
 */
void poc_scene_object_update_bounds(poc_scene_object *obj);

/**
 * @brief Check if object has valid renderable geometry
 *
 * @param obj The scene object
 * @return True if object has a valid mesh, false otherwise
 */
bool poc_scene_object_is_renderable(const poc_scene_object *obj);

/**
 * @brief Add a child object to this object
 *
 * Sets up parent-child relationship. Child transforms become relative to parent.
 *
 * @param parent The parent object
 * @param child The child object to add
 */
void poc_scene_object_add_child(poc_scene_object *parent, poc_scene_object *child);

/**
 * @brief Remove a child object from this object
 *
 * @param parent The parent object
 * @param child The child object to remove
 */
void poc_scene_object_remove_child(poc_scene_object *parent, poc_scene_object *child);

#ifdef __cplusplus
}
#endif