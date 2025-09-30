#include "mesh.h"
#include "poc_engine.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

poc_mesh* poc_mesh_create(void) {
    poc_mesh *mesh = malloc(sizeof(poc_mesh));
    if (!mesh) {
        return NULL;
    }

    memset(mesh, 0, sizeof(poc_mesh));

    // Initialize bounds to invalid values
    glm_vec3_copy((vec3){FLT_MAX, FLT_MAX, FLT_MAX}, mesh->local_aabb_min);
    glm_vec3_copy((vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX}, mesh->local_aabb_max);

    // Initialize material to default values
    mesh->has_material = false;
    memset(&mesh->material, 0, sizeof(poc_material));

    return mesh;
}

poc_mesh* poc_mesh_load(const char *filename) {
    if (!filename) {
        return NULL;
    }

    poc_mesh *mesh = poc_mesh_create();
    if (!mesh) {
        return NULL;
    }

    // Load geometry from OBJ file using the existing OBJ loader
    poc_model model;
    poc_obj_result obj_result = poc_model_load(filename, &model);
    if (obj_result != POC_OBJ_RESULT_SUCCESS) {
        printf("Failed to load OBJ file %s: %s\n", filename, poc_obj_result_to_string(obj_result));
        poc_mesh_destroy(mesh);
        return NULL;
    }

    // Find the first non-empty group in any object
    poc_mesh_group *group = NULL;
    for (uint32_t obj_idx = 0; obj_idx < model.object_count && !group; obj_idx++) {
        for (uint32_t grp_idx = 0; grp_idx < model.objects[obj_idx].group_count; grp_idx++) {
            if (model.objects[obj_idx].groups[grp_idx].vertex_count > 0) {
                group = &model.objects[obj_idx].groups[grp_idx];
                break;
            }
        }
    }

    if (!group) {
        printf("Warning: No geometry found in OBJ file\n");
        poc_model_destroy(&model);
        poc_mesh_destroy(mesh);
        return NULL;
    }

    // Copy the vertex data since the model will be destroyed
    size_t vertex_size = sizeof(poc_vertex) * group->vertex_count;
    size_t index_size = sizeof(uint32_t) * group->index_count;

    poc_vertex *vertex_copy = malloc(vertex_size);
    uint32_t *index_copy = malloc(index_size);

    if (!vertex_copy || (group->index_count > 0 && !index_copy)) {
        free(vertex_copy);
        free(index_copy);
        poc_model_destroy(&model);
        poc_mesh_destroy(mesh);
        return NULL;
    }

    memcpy(vertex_copy, group->vertices, vertex_size);
    if (group->index_count > 0) {
        memcpy(index_copy, group->indices, index_size);
    }

    // Set the geometry data (mesh takes ownership of copied data)
    poc_mesh_set_data(mesh, vertex_copy, group->vertex_count,
                     index_copy, group->index_count, true);

    // Copy material data if the group has a material
    if (group->material_index != UINT32_MAX && group->material_index < model.material_count) {
        mesh->material = model.materials[group->material_index];
        mesh->has_material = true;
        printf("✓ Copied material '%s' for mesh\n", mesh->material.name);
    } else {
        mesh->has_material = false;
        printf("⚠ No material found for mesh group, using default\n");
    }

    poc_model_destroy(&model);

    // Store the asset path for serialization/reference purposes
    if (filename) {
        strncpy(mesh->source_path, filename, POC_ASSET_PATH_MAX - 1);
        mesh->source_path[POC_ASSET_PATH_MAX - 1] = '\0';
    }

    return mesh;
}

void poc_mesh_set_data(poc_mesh *mesh,
                       poc_vertex *vertices, uint32_t vertex_count,
                       uint32_t *indices, uint32_t index_count,
                       bool owns_data) {
    if (!mesh) {
        return;
    }

    // Free existing data if we own it
    if (mesh->owns_data) {
        free(mesh->vertices);
        free(mesh->indices);
    }

    mesh->vertices = vertices;
    mesh->vertex_count = vertex_count;
    mesh->indices = indices;
    mesh->index_count = index_count;
    mesh->owns_data = owns_data;

    // Calculate bounds from the new data
    poc_mesh_calculate_bounds(mesh);
}

void poc_mesh_calculate_bounds(poc_mesh *mesh) {
    if (!mesh || !mesh->vertices || mesh->vertex_count == 0) {
        return;
    }

    // Initialize bounds
    vec3 min_bounds = {FLT_MAX, FLT_MAX, FLT_MAX};
    vec3 max_bounds = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    // Find min/max of all vertex positions
    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        const vec3 *pos = &mesh->vertices[i].position;

        // Update min bounds
        if ((*pos)[0] < min_bounds[0]) min_bounds[0] = (*pos)[0];
        if ((*pos)[1] < min_bounds[1]) min_bounds[1] = (*pos)[1];
        if ((*pos)[2] < min_bounds[2]) min_bounds[2] = (*pos)[2];

        // Update max bounds
        if ((*pos)[0] > max_bounds[0]) max_bounds[0] = (*pos)[0];
        if ((*pos)[1] > max_bounds[1]) max_bounds[1] = (*pos)[1];
        if ((*pos)[2] > max_bounds[2]) max_bounds[2] = (*pos)[2];
    }

    // Store bounds
    glm_vec3_copy(min_bounds, mesh->local_aabb_min);
    glm_vec3_copy(max_bounds, mesh->local_aabb_max);

    // Calculate center
    glm_vec3_add(min_bounds, max_bounds, mesh->center);
    glm_vec3_scale(mesh->center, 0.5f, mesh->center);

    // Calculate bounding radius (distance from center to farthest vertex)
    float max_distance_sq = 0.0f;
    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        vec3 diff;
        glm_vec3_sub(mesh->vertices[i].position, mesh->center, diff);
        float distance_sq = glm_vec3_norm2(diff);
        if (distance_sq > max_distance_sq) {
            max_distance_sq = distance_sq;
        }
    }
    mesh->bounding_radius = sqrtf(max_distance_sq);
}

void poc_mesh_destroy(poc_mesh *mesh) {
    if (!mesh) {
        return;
    }

    // Free vertex/index data if we own it
    if (mesh->owns_data) {
        free(mesh->vertices);
        free(mesh->indices);
    }

    free(mesh);
}

uint32_t poc_mesh_get_triangle_count(const poc_mesh *mesh) {
    if (!mesh) {
        return 0;
    }

    if (mesh->indices && mesh->index_count > 0) {
        return mesh->index_count / 3;
    } else if (mesh->vertices && mesh->vertex_count > 0) {
        return mesh->vertex_count / 3;
    }

    return 0;
}

bool poc_mesh_is_valid(const poc_mesh *mesh) {
    return mesh && mesh->vertices && mesh->vertex_count > 0;
}
