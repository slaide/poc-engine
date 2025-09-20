#include "obj_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cglm/cglm.h>

const char *poc_obj_result_to_string(poc_obj_result result) {
    switch (result) {
        case POC_OBJ_RESULT_SUCCESS: return "Success";
        case POC_OBJ_RESULT_ERROR_FILE_NOT_FOUND: return "File not found";
        case POC_OBJ_RESULT_ERROR_PARSE_FAILED: return "Parse failed";
        case POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case POC_OBJ_RESULT_ERROR_INVALID_FORMAT: return "Invalid format";
        case POC_OBJ_RESULT_ERROR_MTL_NOT_FOUND: return "MTL file not found";
        default: return "Unknown error";
    }
}

static char *extract_directory(const char *filepath) {
    const char *last_slash = strrchr(filepath, '/');
    if (!last_slash) {
        char *dir = malloc(3);
        strcpy(dir, "./");
        return dir;
    }

    size_t dir_len = last_slash - filepath + 1;
    char *dir = malloc(dir_len + 1);
    strncpy(dir, filepath, dir_len);
    dir[dir_len] = '\0';
    return dir;
}

static poc_obj_result parse_mtl_file(const char *mtl_filename, poc_model *model) {
    FILE *file = fopen(mtl_filename, "r");
    if (!file) {
        return POC_OBJ_RESULT_ERROR_MTL_NOT_FOUND;
    }

    char line[1024];
    poc_material *current_material = NULL;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "newmtl ", 7) == 0) {
            // Allocate new material
            model->materials = realloc(model->materials,
                (model->material_count + 1) * sizeof(poc_material));
            if (!model->materials) {
                fclose(file);
                return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
            }

            current_material = &model->materials[model->material_count];
            memset(current_material, 0, sizeof(poc_material));

            // Set default values
            glm_vec3_copy((vec3){0.2f, 0.2f, 0.2f}, current_material->ambient);
            glm_vec3_copy((vec3){0.8f, 0.8f, 0.8f}, current_material->diffuse);
            glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, current_material->specular);
            current_material->shininess = 32.0f;
            current_material->opacity = 1.0f;
            current_material->illum_model = 2;

            strncpy(current_material->name, line + 7, sizeof(current_material->name) - 1);
            model->material_count++;
        } else if (current_material) {
            if (strncmp(line, "Ka ", 3) == 0) {
                sscanf(line + 3, "%f %f %f",
                    &current_material->ambient[0],
                    &current_material->ambient[1],
                    &current_material->ambient[2]);
            } else if (strncmp(line, "Kd ", 3) == 0) {
                sscanf(line + 3, "%f %f %f",
                    &current_material->diffuse[0],
                    &current_material->diffuse[1],
                    &current_material->diffuse[2]);
            } else if (strncmp(line, "Ks ", 3) == 0) {
                sscanf(line + 3, "%f %f %f",
                    &current_material->specular[0],
                    &current_material->specular[1],
                    &current_material->specular[2]);
            } else if (strncmp(line, "Ns ", 3) == 0) {
                sscanf(line + 3, "%f", &current_material->shininess);
            } else if (strncmp(line, "d ", 2) == 0) {
                sscanf(line + 2, "%f", &current_material->opacity);
            } else if (strncmp(line, "illum ", 6) == 0) {
                sscanf(line + 6, "%d", &current_material->illum_model);
            }
        }
    }

    fclose(file);
    return POC_OBJ_RESULT_SUCCESS;
}

static uint32_t find_material_index(const poc_model *model, const char *material_name) {
    for (uint32_t i = 0; i < model->material_count; i++) {
        if (strcmp(model->materials[i].name, material_name) == 0) {
            return i;
        }
    }
    return 0; // Default to first material or 0 if none found
}

void poc_calculate_face_normal(const vec3 v0, const vec3 v1, const vec3 v2, vec3 normal) {
    vec3 edge1, edge2;
    // Manually copy values to avoid const issues
    edge1[0] = v1[0] - v0[0];
    edge1[1] = v1[1] - v0[1];
    edge1[2] = v1[2] - v0[2];

    edge2[0] = v2[0] - v0[0];
    edge2[1] = v2[1] - v0[1];
    edge2[2] = v2[2] - v0[2];

    glm_vec3_crossn(edge1, edge2, normal);
}

void poc_calculate_smooth_normals(poc_model *model) {
    // Reset all normals to zero
    for (uint32_t obj_idx = 0; obj_idx < model->object_count; obj_idx++) {
        poc_mesh_object *object = &model->objects[obj_idx];
        for (uint32_t grp_idx = 0; grp_idx < object->group_count; grp_idx++) {
            poc_mesh_group *group = &object->groups[grp_idx];
            for (uint32_t v_idx = 0; v_idx < group->vertex_count; v_idx++) {
                glm_vec3_zero(group->vertices[v_idx].normal);
            }
        }
    }

    // Accumulate face normals for each vertex
    for (uint32_t obj_idx = 0; obj_idx < model->object_count; obj_idx++) {
        poc_mesh_object *object = &model->objects[obj_idx];
        for (uint32_t grp_idx = 0; grp_idx < object->group_count; grp_idx++) {
            poc_mesh_group *group = &object->groups[grp_idx];

            for (uint32_t i = 0; i < group->index_count; i += 3) {
                uint32_t i0 = group->indices[i];
                uint32_t i1 = group->indices[i + 1];
                uint32_t i2 = group->indices[i + 2];

                vec3 face_normal;
                poc_calculate_face_normal(
                    group->vertices[i0].position,
                    group->vertices[i1].position,
                    group->vertices[i2].position,
                    face_normal
                );

                // Add face normal to each vertex normal
                glm_vec3_add(group->vertices[i0].normal, face_normal, group->vertices[i0].normal);
                glm_vec3_add(group->vertices[i1].normal, face_normal, group->vertices[i1].normal);
                glm_vec3_add(group->vertices[i2].normal, face_normal, group->vertices[i2].normal);
            }
        }
    }

    // Normalize all vertex normals
    for (uint32_t obj_idx = 0; obj_idx < model->object_count; obj_idx++) {
        poc_mesh_object *object = &model->objects[obj_idx];
        for (uint32_t grp_idx = 0; grp_idx < object->group_count; grp_idx++) {
            poc_mesh_group *group = &object->groups[grp_idx];
            for (uint32_t v_idx = 0; v_idx < group->vertex_count; v_idx++) {
                glm_vec3_normalize(group->vertices[v_idx].normal);
            }
        }
    }
}

poc_obj_result poc_model_load(const char *obj_filename, poc_model *model) {
    memset(model, 0, sizeof(poc_model));

    FILE *file = fopen(obj_filename, "r");
    if (!file) {
        return POC_OBJ_RESULT_ERROR_FILE_NOT_FOUND;
    }

    char line[1024];
    char *dir = extract_directory(obj_filename);

    // Current parsing state
    poc_mesh_object *current_object = NULL;
    poc_mesh_group *current_group = NULL;
    uint32_t current_material_index = 0;
    uint32_t current_smoothing_group = 0;

    // Temporary arrays for face parsing
    uint32_t *temp_indices = NULL;
    poc_vertex *temp_vertices = NULL;
    uint32_t temp_vertex_count = 0;
    uint32_t temp_index_count = 0;
    uint32_t temp_capacity = 1024;

    temp_vertices = malloc(temp_capacity * sizeof(poc_vertex));
    temp_indices = malloc(temp_capacity * sizeof(uint32_t));

    if (!temp_vertices || !temp_indices) {
        free(temp_vertices);
        free(temp_indices);
        free(dir);
        fclose(file);
        return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
    }

    // Create default object and group for OBJ files without explicit declarations
    model->objects = malloc(sizeof(poc_mesh_object));
    if (!model->objects) {
        free(temp_vertices);
        free(temp_indices);
        free(dir);
        fclose(file);
        return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
    }

    current_object = &model->objects[0];
    memset(current_object, 0, sizeof(poc_mesh_object));
    strcpy(current_object->name, "default");
    model->object_count = 1;

    current_object->groups = malloc(sizeof(poc_mesh_group));
    if (!current_object->groups) {
        free(model->objects);
        free(temp_vertices);
        free(temp_indices);
        free(dir);
        fclose(file);
        return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
    }

    current_group = &current_object->groups[0];
    memset(current_group, 0, sizeof(poc_mesh_group));
    strcpy(current_group->name, "default");
    current_object->group_count = 1;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "mtllib ", 7) == 0) {
            char mtl_filename[512];
            snprintf(mtl_filename, sizeof(mtl_filename), "%s%s", dir, line + 7);
            poc_obj_result mtl_result = parse_mtl_file(mtl_filename, model);
            if (mtl_result != POC_OBJ_RESULT_SUCCESS) {
                printf("Warning: Could not load MTL file: %s\n", mtl_filename);
            }
        } else if (strncmp(line, "v ", 2) == 0) {
            vec3 pos;
            if (sscanf(line + 2, "%f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
                model->positions = realloc(model->positions,
                    (model->position_count + 1) * sizeof(vec3));
                if (!model->positions) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                glm_vec3_copy(pos, model->positions[model->position_count]);
                model->position_count++;
            }
        } else if (strncmp(line, "vt ", 3) == 0) {
            vec2 tc;
            if (sscanf(line + 3, "%f %f", &tc[0], &tc[1]) == 2) {
                model->texcoords = realloc(model->texcoords,
                    (model->texcoord_count + 1) * sizeof(vec2));
                if (!model->texcoords) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                glm_vec2_copy(tc, model->texcoords[model->texcoord_count]);
                model->texcoord_count++;
            }
        } else if (strncmp(line, "vn ", 3) == 0) {
            vec3 norm;
            if (sscanf(line + 3, "%f %f %f", &norm[0], &norm[1], &norm[2]) == 3) {
                model->normals = realloc(model->normals,
                    (model->normal_count + 1) * sizeof(vec3));
                if (!model->normals) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                glm_vec3_copy(norm, model->normals[model->normal_count]);
                model->normal_count++;
            }
        } else if (strncmp(line, "o ", 2) == 0) {
            // Finalize current group if any
            if (current_group && temp_vertex_count > 0) {
                current_group->vertices = malloc(temp_vertex_count * sizeof(poc_vertex));
                current_group->indices = malloc(temp_index_count * sizeof(uint32_t));
                if (!current_group->vertices || !current_group->indices) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                memcpy(current_group->vertices, temp_vertices, temp_vertex_count * sizeof(poc_vertex));
                memcpy(current_group->indices, temp_indices, temp_index_count * sizeof(uint32_t));
                current_group->vertex_count = temp_vertex_count;
                current_group->index_count = temp_index_count;
                temp_vertex_count = 0;
                temp_index_count = 0;
            }

            // Create new object
            model->objects = realloc(model->objects,
                (model->object_count + 1) * sizeof(poc_mesh_object));
            if (!model->objects) {
                free(temp_vertices);
                free(temp_indices);
                free(dir);
                fclose(file);
                return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
            }

            current_object = &model->objects[model->object_count];
            memset(current_object, 0, sizeof(poc_mesh_object));
            strncpy(current_object->name, line + 2, sizeof(current_object->name) - 1);
            model->object_count++;

            // Create default group for the object
            current_object->groups = malloc(sizeof(poc_mesh_group));
            if (!current_object->groups) {
                free(temp_vertices);
                free(temp_indices);
                free(dir);
                fclose(file);
                return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
            }
            current_group = &current_object->groups[0];
            memset(current_group, 0, sizeof(poc_mesh_group));
            strcpy(current_group->name, "default");
            current_group->material_index = current_material_index;
            current_group->smoothing_group = current_smoothing_group;
            current_object->group_count = 1;
        } else if (strncmp(line, "g ", 2) == 0) {
            // Finalize current group if any
            if (current_group && temp_vertex_count > 0) {
                current_group->vertices = malloc(temp_vertex_count * sizeof(poc_vertex));
                current_group->indices = malloc(temp_index_count * sizeof(uint32_t));
                if (!current_group->vertices || !current_group->indices) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                memcpy(current_group->vertices, temp_vertices, temp_vertex_count * sizeof(poc_vertex));
                memcpy(current_group->indices, temp_indices, temp_index_count * sizeof(uint32_t));
                current_group->vertex_count = temp_vertex_count;
                current_group->index_count = temp_index_count;
                temp_vertex_count = 0;
                temp_index_count = 0;
            }

            // Ensure we have an object
            if (!current_object) {
                model->objects = realloc(model->objects,
                    (model->object_count + 1) * sizeof(poc_mesh_object));
                if (!model->objects) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                current_object = &model->objects[model->object_count];
                memset(current_object, 0, sizeof(poc_mesh_object));
                strcpy(current_object->name, "default");
                model->object_count++;
            }

            // Create new group
            current_object->groups = realloc(current_object->groups,
                (current_object->group_count + 1) * sizeof(poc_mesh_group));
            if (!current_object->groups) {
                free(temp_vertices);
                free(temp_indices);
                free(dir);
                fclose(file);
                return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
            }

            current_group = &current_object->groups[current_object->group_count];
            memset(current_group, 0, sizeof(poc_mesh_group));
            strncpy(current_group->name, line + 2, sizeof(current_group->name) - 1);
            current_group->material_index = current_material_index;
            current_group->smoothing_group = current_smoothing_group;
            current_object->group_count++;
        } else if (strncmp(line, "usemtl ", 7) == 0) {
            current_material_index = find_material_index(model, line + 7);
            if (current_group) {
                current_group->material_index = current_material_index;
            }
        } else if (strncmp(line, "s ", 2) == 0) {
            if (strcmp(line + 2, "off") == 0) {
                current_smoothing_group = 0;
            } else {
                current_smoothing_group = atoi(line + 2);
            }
            if (current_group) {
                current_group->smoothing_group = current_smoothing_group;
            }
        } else if (strncmp(line, "f ", 2) == 0) {
            // Ensure we have an object and group
            if (!current_object) {
                model->objects = realloc(model->objects,
                    (model->object_count + 1) * sizeof(poc_mesh_object));
                if (!model->objects) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                current_object = &model->objects[model->object_count];
                memset(current_object, 0, sizeof(poc_mesh_object));
                strcpy(current_object->name, "default");
                model->object_count++;

                current_object->groups = malloc(sizeof(poc_mesh_group));
                if (!current_object->groups) {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                }
                current_group = &current_object->groups[0];
                memset(current_group, 0, sizeof(poc_mesh_group));
                strcpy(current_group->name, "default");
                current_group->material_index = current_material_index;
                current_group->smoothing_group = current_smoothing_group;
                current_object->group_count = 1;
            }

            // Parse face (supporting triangles only for now)
            char *face_data = line + 2;
            uint32_t face_vertices[3];
            uint32_t vertex_count = 0;

            char *token = strtok(face_data, " ");
            while (token && vertex_count < 3) {
                int v_idx = 0, vt_idx = 0, vn_idx = 0;

                if (sscanf(token, "%d/%d/%d", &v_idx, &vt_idx, &vn_idx) == 3) {
                    // v/vt/vn format
                } else if (sscanf(token, "%d//%d", &v_idx, &vn_idx) == 2) {
                    // v//vn format
                    vt_idx = 0;
                } else if (sscanf(token, "%d/%d", &v_idx, &vt_idx) == 2) {
                    // v/vt format
                    vn_idx = 0;
                } else if (sscanf(token, "%d", &v_idx) == 1) {
                    // v format only
                    vt_idx = 0;
                    vn_idx = 0;
                } else {
                    free(temp_vertices);
                    free(temp_indices);
                    free(dir);
                    fclose(file);
                    return POC_OBJ_RESULT_ERROR_INVALID_FORMAT;
                }

                // Convert to 0-based indices
                v_idx = (v_idx > 0) ? v_idx - 1 : (int)model->position_count + v_idx;
                vt_idx = (vt_idx > 0) ? vt_idx - 1 : (vt_idx < 0) ? (int)model->texcoord_count + vt_idx : -1;
                vn_idx = (vn_idx > 0) ? vn_idx - 1 : (vn_idx < 0) ? (int)model->normal_count + vn_idx : -1;

                // Create vertex
                if (temp_vertex_count >= temp_capacity) {
                    temp_capacity *= 2;
                    temp_vertices = realloc(temp_vertices, temp_capacity * sizeof(poc_vertex));
                    temp_indices = realloc(temp_indices, temp_capacity * sizeof(uint32_t));
                    if (!temp_vertices || !temp_indices) {
                        free(temp_vertices);
                        free(temp_indices);
                        free(dir);
                        fclose(file);
                        return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
                    }
                }

                poc_vertex *vertex = &temp_vertices[temp_vertex_count];

                // Position
                if (v_idx >= 0 && v_idx < (int)model->position_count) {
                    glm_vec3_copy(model->positions[v_idx], vertex->position);
                } else {
                    glm_vec3_zero(vertex->position);
                }

                // Texture coordinates
                if (vt_idx >= 0 && vt_idx < (int)model->texcoord_count) {
                    glm_vec2_copy(model->texcoords[vt_idx], vertex->texcoord);
                } else {
                    glm_vec2_zero(vertex->texcoord);
                }

                // Normal
                if (vn_idx >= 0 && vn_idx < (int)model->normal_count) {
                    glm_vec3_copy(model->normals[vn_idx], vertex->normal);
                } else {
                    glm_vec3_zero(vertex->normal);
                }

                face_vertices[vertex_count] = temp_vertex_count;
                temp_vertex_count++;
                vertex_count++;

                token = strtok(NULL, " ");
            }

            if (vertex_count == 3) {
                temp_indices[temp_index_count++] = face_vertices[0];
                temp_indices[temp_index_count++] = face_vertices[1];
                temp_indices[temp_index_count++] = face_vertices[2];
            } else {
                printf("Warning: Face has %u vertices, expected 3\n", vertex_count);
            }
        }
    }

    // Finalize last group
    if (current_group && temp_vertex_count > 0) {
        current_group->vertices = malloc(temp_vertex_count * sizeof(poc_vertex));
        current_group->indices = malloc(temp_index_count * sizeof(uint32_t));
        if (!current_group->vertices || !current_group->indices) {
            free(temp_vertices);
            free(temp_indices);
            free(dir);
            fclose(file);
            return POC_OBJ_RESULT_ERROR_OUT_OF_MEMORY;
        }
        memcpy(current_group->vertices, temp_vertices, temp_vertex_count * sizeof(poc_vertex));
        memcpy(current_group->indices, temp_indices, temp_index_count * sizeof(uint32_t));
        current_group->vertex_count = temp_vertex_count;
        current_group->index_count = temp_index_count;
    }

    // Calculate smooth normals for groups that don't have explicit normals
    poc_calculate_smooth_normals(model);

    free(temp_vertices);
    free(temp_indices);
    free(dir);
    fclose(file);

    return POC_OBJ_RESULT_SUCCESS;
}

void poc_model_destroy(poc_model *model) {
    if (!model) return;

    // Free objects and groups
    for (uint32_t i = 0; i < model->object_count; i++) {
        poc_mesh_object *object = &model->objects[i];
        for (uint32_t j = 0; j < object->group_count; j++) {
            free(object->groups[j].vertices);
            free(object->groups[j].indices);
        }
        free(object->groups);
    }
    free(model->objects);

    // Free materials
    free(model->materials);

    // Free raw data arrays
    free(model->positions);
    free(model->normals);
    free(model->texcoords);

    memset(model, 0, sizeof(poc_model));
}