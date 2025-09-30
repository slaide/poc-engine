#include "scene.h"
#include "scene_object.h"
#include "mesh.h"
#include "poc_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SCENE_FILE_HEADER "poc_scene"
#define SCENE_FILE_VERSION 1

typedef struct parsed_object {
    uint32_t id;
    uint32_t parent_id;
    bool id_set;
    char name[256];
    float position[3];
    float rotation[3];
    float scale[3];
    bool visible;
    bool enabled;
    char mesh_path[POC_ASSET_PATH_MAX];
} parsed_object;

static char *trim_whitespace(char *str) {
    if (!str) {
        return NULL;
    }

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }

    return str;
}

static bool parse_quoted_string(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) {
        return false;
    }

    const char *cursor = src;
    if (*cursor == '"') {
        cursor++; // Skip opening quote
    }

    size_t out_index = 0;
    while (*cursor && *cursor != '"') {
        char ch = *cursor++;
        if (ch == '\\' && *cursor) {
            ch = *cursor++;
        }

        if (out_index + 1 >= dst_size) {
            return false;
        }

        dst[out_index++] = ch;
    }

    if (*cursor == '"') {
        cursor++;
    }

    dst[out_index] = '\0';
    return true;
}

static void write_quoted_string(FILE *file, const char *key, const char *value) {
    fprintf(file, "%s=\"", key);
    if (value) {
        for (const char *cursor = value; *cursor; ++cursor) {
            if (*cursor == '"' || *cursor == '\\') {
                fputc('\\', file);
            }
            fputc(*cursor, file);
        }
    }
    fprintf(file, "\"\n");
}

static void parsed_object_init(parsed_object *object) {
    memset(object, 0, sizeof(*object));
    object->visible = true;
    object->enabled = true;
    object->scale[0] = 1.0f;
    object->scale[1] = 1.0f;
    object->scale[2] = 1.0f;
}

static bool ensure_mesh_capacity(poc_scene *scene) {
    if (scene->mesh_asset_count < scene->mesh_asset_capacity) {
        return true;
    }

    uint32_t new_capacity = scene->mesh_asset_capacity == 0 ? 8 : scene->mesh_asset_capacity * 2;
    poc_scene_mesh_entry *entries = realloc(scene->mesh_assets,
                                            sizeof(poc_scene_mesh_entry) * new_capacity);
    if (!entries) {
        return false;
    }

    scene->mesh_assets = entries;
    scene->mesh_asset_capacity = new_capacity;
    return true;
}

static poc_mesh* scene_acquire_mesh(poc_scene *scene, const char *path) {
    if (!scene || !path || !path[0]) {
        return NULL;
    }

    for (uint32_t i = 0; i < scene->mesh_asset_count; i++) {
        poc_scene_mesh_entry *entry = &scene->mesh_assets[i];
        if (strcmp(entry->path, path) == 0) {
            entry->ref_count++;
            return entry->mesh;
        }
    }

    poc_mesh *mesh = poc_mesh_load(path);
    if (!mesh) {
        printf("Failed to load mesh '%s' while reading scene file\n", path);
        return NULL;
    }

    if (!ensure_mesh_capacity(scene)) {
        poc_mesh_destroy(mesh);
        return NULL;
    }

    poc_scene_mesh_entry *entry = &scene->mesh_assets[scene->mesh_asset_count++];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->path, path, POC_ASSET_PATH_MAX - 1);
    entry->mesh = mesh;
    entry->ref_count = 1;
    entry->owned = true;

    return mesh;
}

bool poc_scene_save_to_file(const poc_scene *scene, const char *path) {
    if (!scene || !path) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (!file) {
        printf("Failed to open scene file '%s' for writing\n", path);
        return false;
    }

    fprintf(file, "%s v%d\n", SCENE_FILE_HEADER, SCENE_FILE_VERSION);
    fprintf(file, "next_id=%u\n", scene->next_object_id);

    for (uint32_t i = 0; i < scene->object_count; i++) {
        const poc_scene_object *object = scene->objects[i];
        if (!object) {
            continue;
        }

        fprintf(file, "[object]\n");
        fprintf(file, "id=%u\n", object->id);
        write_quoted_string(file, "name", object->name);
        fprintf(file, "position=%.6f %.6f %.6f\n",
                object->position[0], object->position[1], object->position[2]);
        fprintf(file, "rotation=%.6f %.6f %.6f\n",
                object->rotation[0], object->rotation[1], object->rotation[2]);
        fprintf(file, "scale=%.6f %.6f %.6f\n",
                object->scale[0], object->scale[1], object->scale[2]);
        fprintf(file, "visible=%d\n", object->visible ? 1 : 0);
        fprintf(file, "enabled=%d\n", object->enabled ? 1 : 0);
        uint32_t parent_id = object->parent ? object->parent->id : 0;
        fprintf(file, "parent=%u\n", parent_id);

        const char *mesh_path = "";
        if (object->mesh && object->mesh->source_path[0] != '\0') {
            mesh_path = object->mesh->source_path;
        }
        write_quoted_string(file, "mesh", mesh_path);
        fprintf(file, "[end]\n");
    }

    fclose(file);
    return true;
}

poc_scene* poc_scene_load_from_file(const char *path) {
    if (!path) {
        return NULL;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        printf("Failed to open scene file '%s' for reading\n", path);
        return NULL;
    }

    parsed_object *objects = NULL;
    size_t object_count = 0;
    size_t object_capacity = 0;

    char line[1024];
    bool header_seen = false;
    bool in_object = false;
    parsed_object current;
    parsed_object_init(&current);
    uint32_t next_id = 1;
    bool next_id_set = false;

    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim_whitespace(line);
        if (!trimmed || trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        if (!header_seen) {
            int version = 0;
            if (sscanf(trimmed, "%*s v%d", &version) != 1 || strncmp(trimmed, SCENE_FILE_HEADER, strlen(SCENE_FILE_HEADER)) != 0) {
                printf("Invalid scene file header: %s\n", trimmed);
                free(objects);
                fclose(file);
                return NULL;
            }
            header_seen = true;
            continue;
        }

        if (!in_object) {
            if (strncmp(trimmed, "next_id=", 8) == 0) {
                next_id = (uint32_t)strtoul(trimmed + 8, NULL, 10);
                next_id_set = true;
                continue;
            }

            if (strcmp(trimmed, "[object]") == 0) {
                parsed_object_init(&current);
                in_object = true;
                continue;
            }

            // Unknown top-level line, ignore.
            continue;
        }

        if (strcmp(trimmed, "[end]") == 0) {
            if (object_count >= object_capacity) {
                size_t new_capacity = object_capacity == 0 ? 8 : object_capacity * 2;
                parsed_object *new_objects = realloc(objects, new_capacity * sizeof(parsed_object));
                if (!new_objects) {
                    free(objects);
                    fclose(file);
                    return NULL;
                }
                objects = new_objects;
                object_capacity = new_capacity;
            }

            objects[object_count++] = current;
            in_object = false;
            continue;
        }

        char *equals = strchr(trimmed, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char *key = trim_whitespace(trimmed);
        char *value = trim_whitespace(equals + 1);

        if (strcmp(key, "id") == 0) {
            current.id = (uint32_t)strtoul(value, NULL, 10);
            current.id_set = true;
        } else if (strcmp(key, "name") == 0) {
            parse_quoted_string(value, current.name, sizeof(current.name));
        } else if (strcmp(key, "position") == 0) {
            sscanf(value, "%f %f %f", &current.position[0], &current.position[1], &current.position[2]);
        } else if (strcmp(key, "rotation") == 0) {
            sscanf(value, "%f %f %f", &current.rotation[0], &current.rotation[1], &current.rotation[2]);
        } else if (strcmp(key, "scale") == 0) {
            sscanf(value, "%f %f %f", &current.scale[0], &current.scale[1], &current.scale[2]);
        } else if (strcmp(key, "visible") == 0) {
            current.visible = (int)strtol(value, NULL, 10) != 0;
        } else if (strcmp(key, "enabled") == 0) {
            current.enabled = (int)strtol(value, NULL, 10) != 0;
        } else if (strcmp(key, "parent") == 0) {
            current.parent_id = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(key, "mesh") == 0) {
            parse_quoted_string(value, current.mesh_path, sizeof(current.mesh_path));
        }
    }

    fclose(file);

    if (in_object) {
        printf("Scene file '%s' ended before [end]\n", path);
        free(objects);
        return NULL;
    }

    poc_scene *scene = poc_scene_create();
    if (!scene) {
        free(objects);
        return NULL;
    }

    poc_scene_object **created_objects = NULL;
    if (object_count > 0) {
        created_objects = calloc(object_count, sizeof(poc_scene_object*));
        if (!created_objects) {
            poc_scene_destroy(scene, true);
            free(objects);
            return NULL;
        }
    }

    uint32_t max_id = 0;

    for (size_t i = 0; i < object_count; i++) {
        parsed_object *src = &objects[i];

        uint32_t object_id = src->id_set ? src->id : poc_scene_get_next_object_id(scene);
        if (object_id > max_id) {
            max_id = object_id;
        }

        poc_scene_object *obj = poc_scene_object_create(src->name[0] ? src->name : "SceneObject", object_id);
        if (!obj) {
            printf("Failed to create scene object while loading '%s'\n", path);
            poc_scene_destroy(scene, true);
            free(created_objects);
            free(objects);
            return NULL;
        }

        poc_scene_object_set_transform(obj, (vec3){src->position[0], src->position[1], src->position[2]},
                                       (vec3){src->rotation[0], src->rotation[1], src->rotation[2]},
                                       (vec3){src->scale[0], src->scale[1], src->scale[2]});
        obj->visible = src->visible;
        obj->enabled = src->enabled;

        if (src->mesh_path[0] != '\0') {
            poc_mesh *mesh = scene_acquire_mesh(scene, src->mesh_path);
            if (mesh) {
                poc_scene_object_set_mesh(obj, mesh);
            } else {
                printf("Warning: Failed to attach mesh '%s'\n", src->mesh_path);
            }
        }

        if (!poc_scene_add_object(scene, obj)) {
            printf("Failed to add scene object while loading '%s'\n", path);
            poc_scene_object_destroy(obj);
            poc_scene_destroy(scene, true);
            free(created_objects);
            free(objects);
            return NULL;
        }

        created_objects[i] = obj;
    }

    for (size_t i = 0; i < object_count; i++) {
        if (objects[i].parent_id == 0) {
            continue;
        }

        poc_scene_object *child = created_objects[i];
        if (!child) {
            continue;
        }

        poc_scene_object *parent = poc_scene_find_object_by_id(scene, objects[i].parent_id);
        if (parent) {
            poc_scene_object_add_child(parent, child);
        }
    }

    if (next_id_set) {
        scene->next_object_id = next_id;
    } else {
        scene->next_object_id = max_id + 1;
    }

    free(created_objects);
    free(objects);
    return scene;
}

poc_scene* poc_scene_clone(const poc_scene *scene) {
    if (!scene) {
        return NULL;
    }

    poc_scene *clone = poc_scene_create();
    if (!clone) {
        return NULL;
    }

    clone->next_object_id = scene->next_object_id;

    if (scene->object_count == 0) {
        return clone;
    }

    poc_scene_object **created = calloc(scene->object_count, sizeof(poc_scene_object*));
    if (!created) {
        poc_scene_destroy(clone, true);
        return NULL;
    }

    for (uint32_t i = 0; i < scene->object_count; i++) {
        poc_scene_object *src = scene->objects[i];
        if (!src) {
            continue;
        }

        poc_scene_object *dst = poc_scene_object_create(src->name, src->id);
        if (!dst) {
            poc_scene_destroy(clone, true);
            free(created);
            return NULL;
        }

        poc_scene_object_set_transform(dst, src->position, src->rotation, src->scale);
        dst->visible = src->visible;
        dst->enabled = src->enabled;

        if (src->mesh) {
            poc_scene_object_set_mesh(dst, src->mesh);
        }

        if (src->material) {
            poc_scene_object_set_material(dst, src->material);
        }

        if (!poc_scene_add_object(clone, dst)) {
            poc_scene_object_destroy(dst);
            poc_scene_destroy(clone, true);
            free(created);
            return NULL;
        }

        created[i] = dst;
    }

    for (uint32_t i = 0; i < scene->object_count; i++) {
        poc_scene_object *src = scene->objects[i];
        poc_scene_object *dst = created[i];
        if (!src || !dst || !src->parent) {
            continue;
        }

        poc_scene_object *parent_clone = poc_scene_find_object_by_id(clone, src->parent->id);
        if (parent_clone) {
            poc_scene_object_add_child(parent_clone, dst);
        }
    }

    free(created);
    return clone;
}

bool poc_scene_copy_from(poc_scene *dest, const poc_scene *source) {
    if (!dest || !source) {
        return false;
    }

    if (dest == source) {
        return true;
    }

    typedef struct {
        uint32_t id;
        poc_scene_object *object;
        bool processed;
    } dest_entry;

    typedef struct {
        poc_scene_object *child;
        uint32_t parent_id;
    } parent_binding;

    const uint32_t original_count = dest->object_count;
    dest_entry *entries = NULL;
    if (original_count > 0) {
        entries = malloc(sizeof(dest_entry) * original_count);
        if (!entries) {
            return false;
        }

        for (uint32_t i = 0; i < original_count; i++) {
            poc_scene_object *obj = dest->objects[i];
            entries[i].object = obj;
            entries[i].id = obj ? obj->id : 0;
            entries[i].processed = false;
        }
    }

    parent_binding *bindings = NULL;
    uint32_t binding_capacity = source->object_count;
    uint32_t binding_count = 0;
    if (binding_capacity > 0) {
        bindings = malloc(sizeof(parent_binding) * binding_capacity);
        if (!bindings) {
            free(entries);
            return false;
        }
    }

    bool success = true;

    for (uint32_t i = 0; i < source->object_count && success; i++) {
        poc_scene_object *src_obj = source->objects[i];
        if (!src_obj) {
            continue;
        }

        poc_scene_object *dst_obj = NULL;
        uint32_t entry_index = UINT32_MAX;

        for (uint32_t entry = 0; entry < original_count; entry++) {
            if (entries && entries[entry].id == src_obj->id && entries[entry].object) {
                dst_obj = entries[entry].object;
                entries[entry].processed = true;
                entry_index = entry;
                break;
            }
        }

        if (!dst_obj) {
            dst_obj = poc_scene_object_create(src_obj->name, src_obj->id);
            if (!dst_obj) {
                success = false;
                break;
            }

            if (!poc_scene_add_object(dest, dst_obj)) {
                poc_scene_object_destroy(dst_obj);
                success = false;
                break;
            }
        } else {
            strncpy(dst_obj->name, src_obj->name, sizeof(dst_obj->name) - 1);
            dst_obj->name[sizeof(dst_obj->name) - 1] = '\0';
        }

        poc_scene_object_set_transform(dst_obj, src_obj->position, src_obj->rotation, src_obj->scale);
        dst_obj->visible = src_obj->visible;
        dst_obj->enabled = src_obj->enabled;

        if (dst_obj->mesh != src_obj->mesh) {
            poc_scene_object_set_mesh(dst_obj, src_obj->mesh);
        }

        if (dst_obj->material != src_obj->material) {
            poc_scene_object_set_material(dst_obj, src_obj->material);
        }

        if (binding_count < binding_capacity) {
            bindings[binding_count].child = dst_obj;
            bindings[binding_count].parent_id = src_obj->parent ? src_obj->parent->id : 0;
            binding_count++;
        }

        if (entry_index != UINT32_MAX) {
            // Ensure transform matrices are refreshed when reusing existing objects
            poc_scene_object_update_transform(dst_obj);
        }
    }

    if (success && entries) {
        for (uint32_t i = 0; i < original_count; i++) {
            if (!entries[i].processed && entries[i].object) {
                poc_scene_object *obj = entries[i].object;
                poc_scene_remove_object(dest, obj);
                poc_scene_object_destroy(obj);
            }
        }
    }

    if (success) {
        for (uint32_t i = 0; i < dest->object_count; i++) {
            poc_scene_object *obj = dest->objects[i];
            if (!obj) {
                continue;
            }

            if (obj->children) {
                free(obj->children);
            }
            obj->children = NULL;
            obj->child_count = 0;
            obj->child_capacity = 0;
            obj->parent = NULL;
        }

        for (uint32_t i = 0; i < binding_count; i++) {
            parent_binding *binding = &bindings[i];
            if (!binding->child || binding->parent_id == 0) {
                continue;
            }

            poc_scene_object *parent = poc_scene_find_object_by_id(dest, binding->parent_id);
            if (parent) {
                poc_scene_object_add_child(parent, binding->child);
            }
        }
    }

    if (entries) {
        free(entries);
    }
    if (bindings) {
        free(bindings);
    }

    dest->next_object_id = source->next_object_id;

    if (!success) {
        return false;
    }

    if (dest->mesh_assets) {
        for (uint32_t i = 0; i < dest->mesh_asset_count; i++) {
            poc_scene_mesh_entry *entry = &dest->mesh_assets[i];
            if (entry->owned && entry->mesh) {
                poc_mesh_destroy(entry->mesh);
            }
        }
        free(dest->mesh_assets);
        dest->mesh_assets = NULL;
        dest->mesh_asset_capacity = 0;
        dest->mesh_asset_count = 0;
    }

    if (source->mesh_asset_count > 0) {
        dest->mesh_assets = malloc(sizeof(poc_scene_mesh_entry) * source->mesh_asset_count);
        if (!dest->mesh_assets) {
            dest->mesh_asset_capacity = 0;
            dest->mesh_asset_count = 0;
            return false;
        }

        memcpy(dest->mesh_assets, source->mesh_assets,
               sizeof(poc_scene_mesh_entry) * source->mesh_asset_count);
        dest->mesh_asset_count = source->mesh_asset_count;
        dest->mesh_asset_capacity = source->mesh_asset_count;
        for (uint32_t i = 0; i < dest->mesh_asset_count; i++) {
            dest->mesh_assets[i].owned = true;
            source->mesh_assets[i].owned = false;
        }
    } else {
        dest->mesh_assets = NULL;
        dest->mesh_asset_capacity = 0;
        dest->mesh_asset_count = 0;
    }

    return true;
}
