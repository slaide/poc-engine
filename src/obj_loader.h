#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <cglm/cglm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    vec3 position;
    vec3 normal;
    vec2 texcoord;
} poc_vertex;

typedef struct {
    vec3 ambient;    // Ka
    vec3 diffuse;    // Kd
    vec3 specular;   // Ks
    float shininess; // Ns
    float opacity;   // d
    int illum_model; // illum
    char name[256];
} poc_material;

typedef struct {
    poc_vertex *vertices;
    uint32_t *indices;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t material_index;
    uint32_t smoothing_group;
    char name[256];
} poc_mesh_group;

typedef struct {
    poc_mesh_group *groups;
    uint32_t group_count;
    char name[256];
} poc_mesh_object;

typedef struct {
    poc_mesh_object *objects;
    poc_material *materials;
    uint32_t object_count;
    uint32_t material_count;

    // Raw data arrays used during parsing
    vec3 *positions;
    vec3 *normals;
    vec2 *texcoords;
    uint32_t position_count;
    uint32_t normal_count;
    uint32_t texcoord_count;
} poc_model;

typedef enum {
    POC_OBJ_RESULT_SUCCESS = 0,
    POC_OBJ_RESULT_ERROR_FILE_NOT_FOUND,
    POC_OBJ_RESULT_ERROR_PARSE_FAILED,
    POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY,
    POC_OBJ_RESULT_ERROR_INVALID_FORMAT,
    POC_OBJ_RESULT_ERROR_MTL_NOT_FOUND
} poc_obj_result;

poc_obj_result poc_model_load(const char *obj_filename, poc_model *model);
void poc_model_destroy(poc_model *model);
const char *poc_obj_result_to_string(poc_obj_result result);

// Helper functions for normal calculation
void poc_calculate_face_normal(const vec3 v0, const vec3 v1, const vec3 v2, vec3 normal);
void poc_calculate_smooth_normals(poc_model *model);

#ifdef __cplusplus
}
#endif