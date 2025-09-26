/**
 * @file scene.h
 * @brief Scene management system for POC Engine
 *
 * This module provides scene-level management of objects, including
 * object collections, picking functionality, and scene queries.
 */

#pragma once

#include "scene_object.h"
#include <cglm/cglm.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ray structure for intersection testing
 */
typedef struct poc_ray {
    vec3 origin;        /**< Ray origin point */
    vec3 direction;     /**< Ray direction (should be normalized) */
} poc_ray;

/**
 * @brief Hit result from ray intersection
 */
typedef struct poc_hit_result {
    bool hit;                   /**< Whether ray hit anything */
    poc_scene_object *object;   /**< Hit object (NULL if no hit) */
    float distance;             /**< Distance from ray origin to hit point */
    vec3 point;                 /**< World-space hit point on AABB surface */
} poc_hit_result;

/**
 * @brief Scene containing a collection of objects
 */
typedef struct poc_scene {
    poc_scene_object **objects;    /**< Array of scene objects */
    uint32_t object_count;         /**< Number of objects */
    uint32_t object_capacity;      /**< Capacity of objects array */
    uint32_t next_object_id;       /**< Next available object ID */
} poc_scene;

/**
 * @brief Create a new empty scene
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
 * @brief Add an object to the scene
 *
 * The scene does not take ownership of the object.
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
 * @brief Remove an object from the scene by ID
 *
 * @param scene The scene
 * @param id The ID of the object to remove
 * @return Pointer to removed object, or NULL if not found
 */
poc_scene_object* poc_scene_remove_object_by_id(poc_scene *scene, uint32_t id);

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
 * @brief Perform ray-AABB intersection test against an object
 *
 * @param ray The ray to test
 * @param object The object to test against
 * @param hit_result Output hit result
 * @return True if ray hits the object's AABB, false otherwise
 */
bool poc_scene_ray_object_intersection(const poc_ray *ray,
                                       const poc_scene_object *object,
                                       poc_hit_result *hit_result);

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
 * @brief Get all renderable objects in the scene
 *
 * Returns an array of pointers to objects that have valid meshes and are enabled/visible.
 * The returned array is only valid until the next scene modification.
 *
 * @param scene The scene
 * @param out_count Output parameter for number of renderable objects
 * @return Array of renderable object pointers (do not free)
 */
poc_scene_object** poc_scene_get_renderable_objects(poc_scene *scene, uint32_t *out_count);

#ifdef __cplusplus
}
#endif