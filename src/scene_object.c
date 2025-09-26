#include "scene_object.h"
#include "../include/poc_engine.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

// Access to global context for renderable creation
extern poc_context *g_active_context;

poc_scene_object* poc_scene_object_create(const char *name, uint32_t id) {
    poc_scene_object *obj = malloc(sizeof(poc_scene_object));
    if (!obj) {
        return NULL;
    }

    memset(obj, 0, sizeof(poc_scene_object));

    // Set identification
    obj->id = id;
    if (name) {
        strncpy(obj->name, name, sizeof(obj->name) - 1);
        obj->name[sizeof(obj->name) - 1] = '\0';
    }

    // Initialize transform to identity
    glm_vec3_zero(obj->position);
    glm_vec3_zero(obj->rotation);
    glm_vec3_one(obj->scale);
    glm_mat4_identity(obj->transform_matrix);
    obj->transform_dirty = false;

    // Initialize bounds to invalid values
    glm_vec3_copy((vec3){FLT_MAX, FLT_MAX, FLT_MAX}, obj->world_aabb_min);
    glm_vec3_copy((vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX}, obj->world_aabb_max);
    obj->bounds_dirty = true;

    // Set default state
    obj->visible = true;
    obj->enabled = true;

    return obj;
}

void poc_scene_object_destroy(poc_scene_object *obj) {
    if (!obj) {
        return;
    }

    // Remove from parent
    if (obj->parent) {
        poc_scene_object_remove_child(obj->parent, obj);
    }

    // Remove all children (but don't destroy them)
    if (obj->children) {
        for (uint32_t i = 0; i < obj->child_count; i++) {
            if (obj->children[i]) {
                obj->children[i]->parent = NULL;
            }
        }
        free(obj->children);
    }

    // Clean up renderable
    if (obj->renderable && g_active_context) {
        poc_context_destroy_renderable(g_active_context, obj->renderable);
    }

    // Note: We don't destroy mesh or material as they may be shared
    free(obj);
}

void poc_scene_object_set_mesh(poc_scene_object *obj, poc_mesh *mesh) {
    if (!obj) {
        return;
    }

    // Clean up existing renderable if changing mesh
    if (obj->renderable && g_active_context) {
        poc_context_destroy_renderable(g_active_context, obj->renderable);
        obj->renderable = NULL;
    }

    obj->mesh = mesh;
    obj->bounds_dirty = true;

    // Create new renderable if we have a valid mesh and context
    if (mesh && poc_mesh_is_valid(mesh) && g_active_context) {
        obj->renderable = poc_context_create_renderable(g_active_context, obj->name);
        if (obj->renderable) {
            // Load the mesh data into the renderable
            poc_result result = poc_renderable_load_mesh(obj->renderable, mesh);
            if (result != POC_RESULT_SUCCESS) {
                printf("Failed to load mesh into renderable for object %s\n", obj->name);
                poc_context_destroy_renderable(g_active_context, obj->renderable);
                obj->renderable = NULL;
            }
        }
    }

    // Update bounds immediately if we have a valid mesh
    if (mesh && poc_mesh_is_valid(mesh)) {
        poc_scene_object_update_bounds(obj);
    }
}

void poc_scene_object_set_material(poc_scene_object *obj, poc_material *material) {
    if (!obj) {
        return;
    }

    obj->material = material;
}

void poc_scene_object_set_position(poc_scene_object *obj, vec3 position) {
    if (!obj) {
        return;
    }

    glm_vec3_copy(position, obj->position);
    obj->transform_dirty = true;
    obj->bounds_dirty = true;
}

void poc_scene_object_set_rotation(poc_scene_object *obj, vec3 rotation) {
    if (!obj) {
        return;
    }

    glm_vec3_copy(rotation, obj->rotation);
    obj->transform_dirty = true;
    obj->bounds_dirty = true;
}

void poc_scene_object_set_scale(poc_scene_object *obj, vec3 scale) {
    if (!obj) {
        return;
    }

    glm_vec3_copy(scale, obj->scale);
    obj->transform_dirty = true;
    obj->bounds_dirty = true;
}

void poc_scene_object_set_transform(poc_scene_object *obj,
                                    vec3 position,
                                    vec3 rotation,
                                    vec3 scale) {
    if (!obj) {
        return;
    }

    glm_vec3_copy(position, obj->position);
    glm_vec3_copy(rotation, obj->rotation);
    glm_vec3_copy(scale, obj->scale);
    obj->transform_dirty = true;
    obj->bounds_dirty = true;
}

void poc_scene_object_update_transform(poc_scene_object *obj) {
    if (!obj || !obj->transform_dirty) {
        return;
    }

    // Build transform matrix from components
    mat4 translation, rotation_x, rotation_y, rotation_z, scaling, temp;

    // Create component matrices
    glm_translate_make(translation, obj->position);
    glm_rotate_make(rotation_x, glm_rad(obj->rotation[0]), (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate_make(rotation_y, glm_rad(obj->rotation[1]), (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate_make(rotation_z, glm_rad(obj->rotation[2]), (vec3){0.0f, 0.0f, 1.0f});
    glm_scale_make(scaling, obj->scale);

    // Combine: T * R_y * R_x * R_z * S
    glm_mat4_mul(rotation_z, scaling, temp);
    glm_mat4_mul(rotation_x, temp, temp);
    glm_mat4_mul(rotation_y, temp, temp);
    glm_mat4_mul(translation, temp, obj->transform_matrix);

    obj->transform_dirty = false;

    // Update bounds since transform changed
    poc_scene_object_update_bounds(obj);
}

const mat4* poc_scene_object_get_transform_matrix(poc_scene_object *obj) {
    if (!obj) {
        return NULL;
    }

    poc_scene_object_update_transform(obj);
    return &obj->transform_matrix;
}

void poc_scene_object_update_bounds(poc_scene_object *obj) {
    if (!obj || !obj->bounds_dirty || !obj->mesh || !poc_mesh_is_valid(obj->mesh)) {
        return;
    }

    // Ensure transform is up to date
    poc_scene_object_update_transform(obj);

    // Transform the 8 corners of the local AABB to world space
    vec3 corners[8];
    const vec3 *min = &obj->mesh->local_aabb_min;
    const vec3 *max = &obj->mesh->local_aabb_max;

    // Define all 8 corners of the AABB
    glm_vec3_copy((vec3){(*min)[0], (*min)[1], (*min)[2]}, corners[0]);
    glm_vec3_copy((vec3){(*max)[0], (*min)[1], (*min)[2]}, corners[1]);
    glm_vec3_copy((vec3){(*min)[0], (*max)[1], (*min)[2]}, corners[2]);
    glm_vec3_copy((vec3){(*max)[0], (*max)[1], (*min)[2]}, corners[3]);
    glm_vec3_copy((vec3){(*min)[0], (*min)[1], (*max)[2]}, corners[4]);
    glm_vec3_copy((vec3){(*max)[0], (*min)[1], (*max)[2]}, corners[5]);
    glm_vec3_copy((vec3){(*min)[0], (*max)[1], (*max)[2]}, corners[6]);
    glm_vec3_copy((vec3){(*max)[0], (*max)[1], (*max)[2]}, corners[7]);

    // Transform all corners and find new min/max
    vec3 world_min = {FLT_MAX, FLT_MAX, FLT_MAX};
    vec3 world_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 8; i++) {
        vec4 corner_homogeneous = {corners[i][0], corners[i][1], corners[i][2], 1.0f};
        vec4 transformed;

        glm_mat4_mulv(obj->transform_matrix, corner_homogeneous, transformed);

        // Update world bounds
        if (transformed[0] < world_min[0]) world_min[0] = transformed[0];
        if (transformed[1] < world_min[1]) world_min[1] = transformed[1];
        if (transformed[2] < world_min[2]) world_min[2] = transformed[2];

        if (transformed[0] > world_max[0]) world_max[0] = transformed[0];
        if (transformed[1] > world_max[1]) world_max[1] = transformed[1];
        if (transformed[2] > world_max[2]) world_max[2] = transformed[2];
    }

    glm_vec3_copy(world_min, obj->world_aabb_min);
    glm_vec3_copy(world_max, obj->world_aabb_max);
    obj->bounds_dirty = false;
}

bool poc_scene_object_is_renderable(const poc_scene_object *obj) {
    return obj && obj->enabled && obj->visible && obj->mesh && poc_mesh_is_valid(obj->mesh);
}

void poc_scene_object_add_child(poc_scene_object *parent, poc_scene_object *child) {
    if (!parent || !child || child->parent == parent) {
        return;
    }

    // Remove from current parent if any
    if (child->parent) {
        poc_scene_object_remove_child(child->parent, child);
    }

    // Expand children array if needed
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        poc_scene_object **new_children = realloc(parent->children,
                                                  sizeof(poc_scene_object*) * new_capacity);
        if (!new_children) {
            return; // Failed to allocate
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    // Add child
    parent->children[parent->child_count] = child;
    parent->child_count++;
    child->parent = parent;

    // Child transform becomes relative to parent
    child->transform_dirty = true;
    child->bounds_dirty = true;
}

void poc_scene_object_remove_child(poc_scene_object *parent, poc_scene_object *child) {
    if (!parent || !child || child->parent != parent) {
        return;
    }

    // Find child in array
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            // Remove by shifting remaining elements
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            child->parent = NULL;
            break;
        }
    }
}