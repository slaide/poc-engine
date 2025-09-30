#include "scene.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

poc_scene* poc_scene_create(void) {
    poc_scene *scene = malloc(sizeof(poc_scene));
    if (!scene) {
        return NULL;
    }

    memset(scene, 0, sizeof(poc_scene));
    scene->next_object_id = 1;

    return scene;
}

void poc_scene_destroy(poc_scene *scene, bool destroy_objects) {
    if (!scene) {
        return;
    }

    if (destroy_objects && scene->objects) {
        for (uint32_t i = 0; i < scene->object_count; i++) {
            if (scene->objects[i]) {
                poc_scene_object_destroy(scene->objects[i]);
            }
        }
    }

    if (scene->mesh_assets) {
        for (uint32_t i = 0; i < scene->mesh_asset_count; i++) {
            poc_scene_mesh_entry *entry = &scene->mesh_assets[i];
            if (entry->owned && entry->mesh) {
                poc_mesh_destroy(entry->mesh);
            }
        }
        free(scene->mesh_assets);
    }

    free(scene->objects);
    free(scene);
}

bool poc_scene_add_object(poc_scene *scene, poc_scene_object *object) {
    if (!scene || !object) {
        return false;
    }

    // Expand array if needed
    if (scene->object_count >= scene->object_capacity) {
        uint32_t new_capacity = scene->object_capacity == 0 ? 16 : scene->object_capacity * 2;
        poc_scene_object **new_objects = realloc(scene->objects,
                                                 sizeof(poc_scene_object*) * new_capacity);
        if (!new_objects) {
            return false;
        }
        scene->objects = new_objects;
        scene->object_capacity = new_capacity;
    }

    scene->objects[scene->object_count] = object;
    scene->object_count++;

    return true;
}

bool poc_scene_remove_object(poc_scene *scene, poc_scene_object *object) {
    if (!scene || !object) {
        return false;
    }

    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] == object) {
            // Remove by shifting remaining elements
            for (uint32_t j = i; j < scene->object_count - 1; j++) {
                scene->objects[j] = scene->objects[j + 1];
            }
            scene->object_count--;
            return true;
        }
    }

    return false;
}

poc_scene_object* poc_scene_remove_object_by_id(poc_scene *scene, uint32_t id) {
    if (!scene) {
        return NULL;
    }

    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] && scene->objects[i]->id == id) {
            poc_scene_object *object = scene->objects[i];

            // Remove by shifting remaining elements
            for (uint32_t j = i; j < scene->object_count - 1; j++) {
                scene->objects[j] = scene->objects[j + 1];
            }
            scene->object_count--;

            return object;
        }
    }

    return NULL;
}

poc_scene_object* poc_scene_find_object_by_id(poc_scene *scene, uint32_t id) {
    if (!scene) {
        return NULL;
    }

    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] && scene->objects[i]->id == id) {
            return scene->objects[i];
        }
    }

    return NULL;
}

uint32_t poc_scene_get_next_object_id(poc_scene *scene) {
    if (!scene) {
        return 0;
    }

    return scene->next_object_id++;
}

void poc_scene_update(poc_scene *scene) {
    if (!scene) {
        return;
    }

    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i]) {
            poc_scene_object_update_transform(scene->objects[i]);
        }
    }
}

bool poc_scene_ray_object_intersection(const poc_ray *ray,
                                       const poc_scene_object *object,
                                       poc_hit_result *hit_result) {
    if (!ray || !object || !hit_result || !poc_scene_object_is_renderable(object)) {
        if (hit_result) {
            hit_result->hit = false;
        }
        return false;
    }

    // Ensure object bounds are up to date
    poc_scene_object_update_bounds((poc_scene_object*)object);

    const vec3 *aabb_min = &object->world_aabb_min;
    const vec3 *aabb_max = &object->world_aabb_max;

    // Ray-AABB intersection using slab method
    vec3 inv_dir;
    for (int i = 0; i < 3; i++) {
        if (fabsf(ray->direction[i]) < 1e-6f) {
            inv_dir[i] = 1e6f; // Very large number to handle near-zero directions
        } else {
            inv_dir[i] = 1.0f / ray->direction[i];
        }
    }

    float t_min = -FLT_MAX;
    float t_max = FLT_MAX;

    for (int i = 0; i < 3; i++) {
        float t1 = ((*aabb_min)[i] - ray->origin[i]) * inv_dir[i];
        float t2 = ((*aabb_max)[i] - ray->origin[i]) * inv_dir[i];

        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }

        t_min = fmaxf(t_min, t1);
        t_max = fminf(t_max, t2);

        if (t_min > t_max) {
            hit_result->hit = false;
            return false;
        }
    }

    // Check if intersection is in front of ray origin
    if (t_max < 0.0f) {
        hit_result->hit = false;
        return false;
    }

    // Use t_min if it's positive, otherwise t_max (ray starts inside AABB)
    float t_hit = t_min >= 0.0f ? t_min : t_max;

    hit_result->hit = true;
    hit_result->object = (poc_scene_object*)object;
    hit_result->distance = t_hit;

    // Calculate hit point
    for (int i = 0; i < 3; i++) {
        hit_result->point[i] = ray->origin[i] + t_hit * ray->direction[i];
    }

    return true;
}

bool poc_scene_pick_object(poc_scene *scene,
                          const poc_ray *ray,
                          poc_hit_result *hit_result) {
    if (!scene || !ray || !hit_result) {
        if (hit_result) {
            hit_result->hit = false;
        }
        return false;
    }

    poc_hit_result closest_hit = {.hit = false, .distance = FLT_MAX};
    poc_hit_result current_hit;

    // Test ray against all objects
    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] && poc_scene_ray_object_intersection(ray, scene->objects[i], &current_hit)) {
            if (current_hit.hit && current_hit.distance < closest_hit.distance) {
                closest_hit = current_hit;
            }
        }
    }

    *hit_result = closest_hit;
    return closest_hit.hit;
}

poc_scene_object** poc_scene_get_renderable_objects(poc_scene *scene, uint32_t *out_count) {
    if (!scene || !out_count) {
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    static poc_scene_object **renderable_objects = NULL;
    static uint32_t renderable_capacity = 0;

    // Count renderable objects
    uint32_t renderable_count = 0;
    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] && poc_scene_object_is_renderable(scene->objects[i])) {
            renderable_count++;
        }
    }

    // Expand static array if needed
    if (renderable_count > renderable_capacity) {
        free(renderable_objects);
        renderable_capacity = renderable_count + 16; // Add some extra space
        renderable_objects = malloc(sizeof(poc_scene_object*) * renderable_capacity);
        if (!renderable_objects) {
            renderable_capacity = 0;
            *out_count = 0;
            return NULL;
        }
    }

    // Fill array with renderable objects
    uint32_t index = 0;
    for (uint32_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] && poc_scene_object_is_renderable(scene->objects[i])) {
            renderable_objects[index++] = scene->objects[i];
        }
    }

    *out_count = renderable_count;
    return renderable_objects;
}
