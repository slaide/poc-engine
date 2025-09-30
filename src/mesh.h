/**
 * @file mesh.h
 * @brief Mesh data structure and management for POC Engine
 *
 * This module provides mesh representation that is independent of the rendering backend.
 * Meshes contain geometry data and local space bounding information.
 */

#pragma once

#include <cglm/cglm.h>
#include <stdint.h>
#include <stdbool.h>
#include "poc_engine.h"
#include "obj_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mesh data structure containing geometry and bounds
 *
 * Represents a 3D mesh with vertices, indices, and precomputed bounding information.
 * This is rendering-backend agnostic.
 */
typedef struct poc_mesh {
    // Geometry data
    poc_vertex *vertices;       /**< Array of vertices */
    uint32_t *indices;          /**< Array of vertex indices */
    uint32_t vertex_count;      /**< Number of vertices */
    uint32_t index_count;       /**< Number of indices */

    // Local space bounds (calculated once from vertices)
    vec3 local_aabb_min;        /**< Minimum corner of axis-aligned bounding box */
    vec3 local_aabb_max;        /**< Maximum corner of axis-aligned bounding box */

    // Additional mesh properties
    vec3 center;                /**< Geometric center of the mesh */
    float bounding_radius;      /**< Radius of bounding sphere from center */

    // Material data
    poc_material material;      /**< Material properties for rendering */
    bool has_material;          /**< Whether this mesh has valid material data */

    // Resource management
    bool owns_data;             /**< Whether this mesh owns the vertex/index data */

    // Metadata
    char source_path[POC_ASSET_PATH_MAX]; /**< Source asset path used to create mesh */
} poc_mesh;

/**
 * @brief Create a new empty mesh
 *
 * Creates an uninitialized mesh structure. Geometry data must be set separately.
 *
 * @return Pointer to new mesh, or NULL on failure
 */
poc_mesh* poc_mesh_create(void);

/**
 * @brief Load mesh from OBJ file
 *
 * Loads geometry data from an OBJ file and calculates bounding information.
 *
 * @param filename Path to the OBJ file
 * @return Pointer to loaded mesh, or NULL on failure
 */
poc_mesh* poc_mesh_load(const char *filename);

/**
 * @brief Set mesh geometry data
 *
 * Sets the vertex and index data for a mesh and calculates bounds.
 * The mesh will take ownership of the data if owns_data is true.
 *
 * @param mesh The mesh to modify
 * @param vertices Array of vertices
 * @param vertex_count Number of vertices
 * @param indices Array of indices (can be NULL for non-indexed geometry)
 * @param index_count Number of indices (0 for non-indexed geometry)
 * @param owns_data Whether the mesh should take ownership of the data
 */
void poc_mesh_set_data(poc_mesh *mesh,
                       poc_vertex *vertices, uint32_t vertex_count,
                       uint32_t *indices, uint32_t index_count,
                       bool owns_data);

/**
 * @brief Calculate bounding information from mesh vertices
 *
 * Recalculates the AABB, center, and bounding radius from the current vertex data.
 * This is called automatically when setting mesh data.
 *
 * @param mesh The mesh to calculate bounds for
 */
void poc_mesh_calculate_bounds(poc_mesh *mesh);

/**
 * @brief Destroy a mesh and free its resources
 *
 * Frees the mesh structure and optionally frees vertex/index data if owned.
 *
 * @param mesh The mesh to destroy
 */
void poc_mesh_destroy(poc_mesh *mesh);

/**
 * @brief Get the number of triangles in the mesh
 *
 * @param mesh The mesh to query
 * @return Number of triangles (index_count / 3 or vertex_count / 3)
 */
uint32_t poc_mesh_get_triangle_count(const poc_mesh *mesh);

/**
 * @brief Check if mesh has valid geometry data
 *
 * @param mesh The mesh to check
 * @return True if mesh has vertices, false otherwise
 */
bool poc_mesh_is_valid(const poc_mesh *mesh);

#ifdef __cplusplus
}
#endif
