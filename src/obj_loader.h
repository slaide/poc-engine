/**
 * @file obj_loader.h
 * @brief OBJ/MTL file loader for 3D models
 *
 * This module provides functionality to load 3D models from Wavefront OBJ files
 * along with their associated MTL (material) files. It supports the most common
 * OBJ features including vertices, normals, texture coordinates, faces, objects,
 * groups, smoothing groups, and materials.
 *
 * @section supported_features Supported OBJ Features
 * - Vertex positions (v)
 * - Vertex normals (vn)
 * - Texture coordinates (vt)
 * - Faces with indices (f)
 * - Objects (o)
 * - Groups (g)
 * - Smoothing groups (s)
 * - Material library references (mtllib)
 * - Material usage (usemtl)
 *
 * @section supported_mtl Supported MTL Features
 * - Ambient color (Ka)
 * - Diffuse color (Kd)
 * - Specular color (Ks)
 * - Shininess/specular exponent (Ns)
 * - Opacity/transparency (d)
 * - Illumination model (illum)
 *
 * @section memory_management Memory Management
 * All loaded models must be freed using poc_model_destroy() to prevent memory leaks.
 * The caller is responsible for calling this function when the model is no longer needed.
 *
 * @example
 * @code
 * poc_model model;
 * poc_obj_result result = poc_model_load("cube.obj", &model);
 * if (result == POC_OBJ_RESULT_SUCCESS) {
 *     // Use the model for rendering...
 *     poc_model_destroy(&model);
 * }
 * @endcode
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <cglm/cglm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Vertex data structure for 3D models
 *
 * Contains all the vertex attributes needed for rendering a 3D model.
 * This structure is optimized for graphics pipeline usage.
 */
typedef struct {
    vec3 position;  /**< 3D position coordinates (x, y, z) */
    vec3 normal;    /**< Surface normal vector (normalized) */
    vec2 texcoord;  /**< Texture coordinates (u, v) in range [0, 1] */
} poc_vertex;

/**
 * @brief Material properties for surface appearance
 *
 * Defines the visual properties of a material as specified in MTL files.
 * Used for Phong/Blinn-Phong lighting calculations.
 */
typedef struct {
    vec3 ambient;       /**< Ambient color (Ka) - base color in shadow */
    vec3 diffuse;       /**< Diffuse color (Kd) - main surface color */
    vec3 specular;      /**< Specular color (Ks) - reflection highlight color */
    float shininess;    /**< Shininess exponent (Ns) - specular highlight size */
    float opacity;      /**< Opacity (d) - 1.0 = opaque, 0.0 = transparent */
    int illum_model;    /**< Illumination model (illum) - lighting calculation type */
    char name[256];     /**< Material name for identification */
} poc_material;

/**
 * @brief A group of mesh data within an object
 *
 * Groups are subdivisions within objects that can have different materials
 * and smoothing settings. They correspond to 'g' commands in OBJ files.
 */
typedef struct {
    poc_vertex *vertices;       /**< Array of vertices for this group */
    uint32_t *indices;          /**< Array of face indices for this group */
    uint32_t vertex_count;      /**< Number of vertices in the array */
    uint32_t index_count;       /**< Number of indices in the array */
    uint32_t material_index;    /**< Index into the materials array (UINT32_MAX if none) */
    uint32_t smoothing_group;   /**< Smoothing group ID (0 = no smoothing) */
    char name[256];             /**< Group name from OBJ file */
} poc_mesh_group;

/**
 * @brief A named object containing one or more mesh groups
 *
 * Objects represent distinct 3D entities in the model and correspond to
 * 'o' commands in OBJ files. Each object contains one or more groups.
 */
typedef struct {
    poc_mesh_group *groups;     /**< Array of mesh groups in this object */
    uint32_t group_count;       /**< Number of groups in the array */
    char name[256];             /**< Object name from OBJ file */
} poc_mesh_object;

/**
 * @brief Complete 3D model loaded from an OBJ file
 *
 * This is the top-level structure containing all data for a loaded 3D model.
 * It includes objects, groups, materials, and the raw vertex data used during parsing.
 */
typedef struct {
    poc_mesh_object *objects;       /**< Array of objects in the model */
    poc_material *materials;        /**< Array of materials referenced by groups */
    uint32_t object_count;          /**< Number of objects in the array */
    uint32_t material_count;        /**< Number of materials in the array */

    // Raw data arrays used during parsing - do not use directly for rendering
    vec3 *positions;                /**< Raw position data from OBJ file */
    vec3 *normals;                  /**< Raw normal data from OBJ file */
    vec2 *texcoords;                /**< Raw texture coordinate data from OBJ file */
    uint32_t position_count;        /**< Number of raw positions */
    uint32_t normal_count;          /**< Number of raw normals */
    uint32_t texcoord_count;        /**< Number of raw texture coordinates */
} poc_model;

/**
 * @brief Result codes for OBJ loading operations
 *
 * These codes indicate the success or failure of OBJ/MTL file loading operations.
 * Use poc_obj_result_to_string() to get human-readable descriptions.
 */
typedef enum {
    POC_OBJ_RESULT_SUCCESS = 0,                 /**< Operation completed successfully */
    POC_OBJ_RESULT_ERROR_FILE_NOT_FOUND,        /**< OBJ file could not be opened */
    POC_OBJ_RESULT_ERROR_PARSE_FAILED,          /**< Error parsing OBJ or MTL file syntax */
    POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY,         /**< Insufficient memory to load model */
    POC_OBJ_RESULT_ERROR_INVALID_FORMAT,        /**< File format is invalid or unsupported */
    POC_OBJ_RESULT_ERROR_MTL_NOT_FOUND          /**< Referenced MTL file could not be found */
} poc_obj_result;

/**
 * @brief Load a 3D model from an OBJ file
 *
 * Loads a complete 3D model from the specified Wavefront OBJ file.
 * Automatically loads any referenced MTL files from the same directory.
 *
 * @param obj_filename Path to the OBJ file to load. Must not be NULL.
 * @param model Pointer to poc_model structure to populate. Must not be NULL.
 *              Will be initialized by this function on success.
 * @return POC_OBJ_RESULT_SUCCESS on success, or an error code on failure
 *
 * @note The model structure will be initialized even on failure (safe to call poc_model_destroy).
 * @note MTL files are expected to be in the same directory as the OBJ file.
 * @warning Must call poc_model_destroy() when done to free allocated memory.
 *
 * @example
 * @code
 * poc_model model;
 * poc_obj_result result = poc_model_load("models/cube.obj", &model);
 * if (result != POC_OBJ_RESULT_SUCCESS) {
 *     printf("Failed to load model: %s\n", poc_obj_result_to_string(result));
 * }
 * // ... use model ...
 * poc_model_destroy(&model);
 * @endcode
 */
poc_obj_result poc_model_load(const char *obj_filename, poc_model *model);

/**
 * @brief Free all memory associated with a loaded model
 *
 * Frees all dynamically allocated memory in the model structure.
 * The model structure itself is not freed (it may be stack-allocated).
 *
 * @param model Pointer to the model to destroy. Can be NULL (no-op).
 *              Safe to call multiple times on the same model.
 *
 * @note After calling this function, the model structure should not be used
 *       unless it is reloaded with poc_model_load().
 */
void poc_model_destroy(poc_model *model);

/**
 * @brief Get a human-readable string for an OBJ result code
 *
 * Converts a poc_obj_result error code into a descriptive string for
 * logging or error reporting.
 *
 * @param result The result code to convert
 * @return Pointer to a static string describing the result.
 *         Never returns NULL.
 *
 * @note The returned string is static and should not be freed.
 */
const char *poc_obj_result_to_string(poc_obj_result result);

/**
 * @brief Calculate the face normal for a triangle
 *
 * Computes the surface normal vector for a triangle defined by three vertices.
 * The normal is calculated using the cross product and is automatically normalized.
 *
 * @param v0 First vertex position
 * @param v1 Second vertex position
 * @param v2 Third vertex position
 * @param normal Output normal vector (will be normalized)
 *
 * @note The vertices should be in counter-clockwise order for correct normal direction.
 * @note This is a utility function used internally but may be useful for applications.
 */
void poc_calculate_face_normal(const vec3 v0, const vec3 v1, const vec3 v2, vec3 normal);

/**
 * @brief Calculate smooth vertex normals for the entire model
 *
 * Computes smooth vertex normals by averaging face normals for vertices that
 * are in the same smoothing group. Vertices in smoothing group 0 get face normals
 * (hard edges), while other smoothing groups get averaged normals (smooth edges).
 *
 * @param model Pointer to the model to process. Must not be NULL and must be
 *              a valid loaded model.
 *
 * @note This function modifies the normal vectors in the model's vertex data.
 * @note Smoothing groups are specified by 's' commands in OBJ files.
 * @note This is called automatically during model loading if smoothing groups are present.
 */
void poc_calculate_smooth_normals(poc_model *model);

#ifdef __cplusplus
}
#endif