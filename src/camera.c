#include "camera.h"
#include "scene.h"
#include <math.h>
#include <string.h>

// Default camera parameters
#define DEFAULT_YAW         -90.0f
#define DEFAULT_PITCH        0.0f
#define DEFAULT_ROLL         0.0f
#define DEFAULT_SPEED        2.5f
#define DEFAULT_SENSITIVITY  0.1f
#define DEFAULT_ZOOM         45.0f
#define DEFAULT_NEAR         0.1f
#define DEFAULT_FAR          100.0f

void poc_camera_update_vectors(poc_camera *camera);
static void calculate_view_matrix(poc_camera *camera);
static void calculate_projection_matrix(poc_camera *camera);

static float clamp_vertical_fov(float fov) {
    if (fov < 1.0f) {
        return 1.0f;
    }
    if (fov > 179.0f) {
        return 179.0f;
    }
    return fov;
}

static float clamp_horizontal_fov(float fov) {
    if (fov < 1.0f) {
        return 1.0f;
    }
    if (fov > 179.0f) {
        return 179.0f;
    }
    return fov;
}

static float vertical_to_horizontal(float vertical_fov, float aspect_ratio) {
    if (aspect_ratio <= 0.0f) {
        return clamp_horizontal_fov(vertical_fov);
    }

    float vertical_rad = glm_rad(clamp_vertical_fov(vertical_fov));
    float horiz_rad = 2.0f * atanf(tanf(vertical_rad * 0.5f) * aspect_ratio);
    return clamp_horizontal_fov(glm_deg(horiz_rad));
}

static float horizontal_to_vertical(float horizontal_fov, float aspect_ratio) {
    if (aspect_ratio <= 0.0f) {
        return clamp_vertical_fov(horizontal_fov);
    }

    float horizontal_rad = glm_rad(clamp_horizontal_fov(horizontal_fov));
    float vertical_rad = 2.0f * atanf(tanf(horizontal_rad * 0.5f) / aspect_ratio);
    return clamp_vertical_fov(glm_deg(vertical_rad));
}

poc_camera poc_camera_create(poc_camera_type type, float aspect_ratio) {
    poc_camera camera = {0};

    camera.type = type;
    camera.enabled = true;

    // Initialize position and orientation
    glm_vec3_copy((vec3){0.0f, 0.0f, 3.0f}, camera.position);
    glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, camera.world_up);

    camera.yaw = DEFAULT_YAW;
    camera.pitch = DEFAULT_PITCH;
    camera.roll = DEFAULT_ROLL;

    // Initialize projection parameters
    camera.aspect_ratio = aspect_ratio;
    if (camera.aspect_ratio <= 0.0f) {
        camera.aspect_ratio = 1.0f;
    }
    camera.fov = clamp_vertical_fov(DEFAULT_ZOOM);
    camera.near_plane = DEFAULT_NEAR;
    camera.far_plane = DEFAULT_FAR;
    camera.horizontal_fov = vertical_to_horizontal(camera.fov, camera.aspect_ratio);
    camera.fov_mode = POC_CAMERA_FOV_VERTICAL;

    // Initialize type-specific parameters
    switch (type) {
        case POC_CAMERA_FIRST_PERSON:
            camera.fps.movement_speed = DEFAULT_SPEED;
            camera.fps.mouse_sensitivity = DEFAULT_SENSITIVITY;
            camera.fps.constrain_pitch = true;
            break;

        case POC_CAMERA_ORBIT:
            glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, camera.orbit.target);
            camera.orbit.distance = 5.0f;
            camera.orbit.zoom_speed = 1.0f;
            camera.orbit.orbit_speed = 1.0f;
            camera.orbit.min_distance = 1.0f;
            camera.orbit.max_distance = 20.0f;
            break;

        case POC_CAMERA_FREE:
            camera.free.movement_speed = DEFAULT_SPEED;
            camera.free.rotation_speed = 90.0f; // degrees per second
            break;
    }

    // Initialize input state
    memset(camera.input.keys_pressed, 0, sizeof(camera.input.keys_pressed));
    camera.input.first_mouse = true;
    camera.input.delta_time = 0.0;

    // Update camera vectors and matrices
    camera.matrices_dirty = true;
    poc_camera_update_vectors(&camera);
    poc_camera_update_matrices(&camera);

    return camera;
}

poc_camera poc_camera_create_fps(vec3 position, float yaw, float pitch, float aspect_ratio) {
    poc_camera camera = poc_camera_create(POC_CAMERA_FIRST_PERSON, aspect_ratio);

    glm_vec3_copy(position, camera.position);
    camera.yaw = yaw;
    camera.pitch = pitch;

    camera.matrices_dirty = true;
    poc_camera_update_vectors(&camera);
    poc_camera_update_matrices(&camera);

    return camera;
}

poc_camera poc_camera_create_orbit(vec3 target, float distance, float yaw, float pitch, float aspect_ratio) {
    poc_camera camera = poc_camera_create(POC_CAMERA_ORBIT, aspect_ratio);

    glm_vec3_copy(target, camera.orbit.target);
    camera.orbit.distance = distance;
    camera.yaw = yaw;
    camera.pitch = pitch;

    // Calculate position based on target, distance, and angles
    float x = camera.orbit.target[0] + distance * cosf(glm_rad(yaw)) * cosf(glm_rad(pitch));
    float y = camera.orbit.target[1] + distance * sinf(glm_rad(pitch));
    float z = camera.orbit.target[2] + distance * sinf(glm_rad(yaw)) * cosf(glm_rad(pitch));

    camera.position[0] = x;
    camera.position[1] = y;
    camera.position[2] = z;

    camera.matrices_dirty = true;
    poc_camera_update_vectors(&camera);
    poc_camera_update_matrices(&camera);

    return camera;
}

void poc_camera_update_matrices(poc_camera *camera) {
    if (!camera) return;

    if (camera->matrices_dirty) {
        calculate_view_matrix(camera);
        calculate_projection_matrix(camera);
        camera->matrices_dirty = false;
    }
}

void poc_camera_set_position(poc_camera *camera, vec3 position) {
    if (!camera) return;

    glm_vec3_copy(position, camera->position);
    camera->matrices_dirty = true;
}

void poc_camera_set_rotation(poc_camera *camera, float yaw, float pitch, float roll) {
    if (!camera) return;

    camera->yaw = yaw;
    camera->pitch = pitch;
    camera->roll = roll;

    poc_camera_update_vectors(camera);
    camera->matrices_dirty = true;
}

void poc_camera_set_fov(poc_camera *camera, float fov) {
    poc_camera_set_vertical_fov(camera, fov);
}

void poc_camera_set_vertical_fov(poc_camera *camera, float fov) {
    if (!camera) {
        return;
    }

    camera->fov = clamp_vertical_fov(fov);
    camera->horizontal_fov = vertical_to_horizontal(camera->fov, camera->aspect_ratio);
    camera->fov_mode = POC_CAMERA_FOV_VERTICAL;
    camera->matrices_dirty = true;
}

void poc_camera_set_horizontal_fov(poc_camera *camera, float fov) {
    if (!camera) {
        return;
    }

    camera->horizontal_fov = clamp_horizontal_fov(fov);
    camera->fov = horizontal_to_vertical(camera->horizontal_fov, camera->aspect_ratio);
    camera->fov_mode = POC_CAMERA_FOV_HORIZONTAL;
    camera->matrices_dirty = true;
}

float poc_camera_get_vertical_fov(const poc_camera *camera) {
    if (!camera) {
        return DEFAULT_ZOOM;
    }
    return camera->fov;
}

float poc_camera_get_horizontal_fov(const poc_camera *camera) {
    if (!camera) {
        return vertical_to_horizontal(DEFAULT_ZOOM, 1.0f);
    }
    return camera->horizontal_fov;
}

void poc_camera_set_aspect_ratio(poc_camera *camera, float aspect_ratio) {
    if (!camera) return;

    camera->aspect_ratio = aspect_ratio;
    if (camera->aspect_ratio <= 0.0f) {
        camera->aspect_ratio = 1.0f;
    }

    if (camera->fov_mode == POC_CAMERA_FOV_HORIZONTAL) {
        camera->fov = horizontal_to_vertical(camera->horizontal_fov, camera->aspect_ratio);
    } else {
        camera->horizontal_fov = vertical_to_horizontal(camera->fov, camera->aspect_ratio);
    }

    camera->matrices_dirty = true;
}

const float *poc_camera_get_view_matrix(poc_camera *camera) {
    if (!camera) return NULL;

    poc_camera_update_matrices(camera);
    return (const float *)camera->view_matrix;
}

const float *poc_camera_get_projection_matrix(poc_camera *camera) {
    if (!camera) return NULL;

    poc_camera_update_matrices(camera);
    return (const float *)camera->projection_matrix;
}

void poc_camera_process_keyboard(poc_camera *camera, podi_key key, bool pressed, double delta_time) {
    if (!camera || !camera->enabled) return;

    // Update key state
    if (key >= 0 && key < 64) {
        camera->input.keys_pressed[key] = pressed;
    }

    camera->input.delta_time = delta_time;

    // Process movement based on camera type
    float velocity = 0.0f;
    vec3 movement = {0.0f, 0.0f, 0.0f};

    switch (camera->type) {
        case POC_CAMERA_FIRST_PERSON:
            velocity = camera->fps.movement_speed * (float)delta_time;

            if (camera->input.keys_pressed[PODI_KEY_W]) {
                glm_vec3_muladds(camera->front, velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_S]) {
                glm_vec3_muladds(camera->front, -velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_A]) {
                glm_vec3_muladds(camera->right, -velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_D]) {
                glm_vec3_muladds(camera->right, velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_SPACE]) {
                glm_vec3_muladds(camera->world_up, velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_SHIFT]) {
                glm_vec3_muladds(camera->world_up, -velocity, movement);
            }
            break;

        case POC_CAMERA_FREE:
            velocity = camera->free.movement_speed * (float)delta_time;

            if (camera->input.keys_pressed[PODI_KEY_W]) {
                glm_vec3_muladds(camera->front, velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_S]) {
                glm_vec3_muladds(camera->front, -velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_A]) {
                glm_vec3_muladds(camera->right, -velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_D]) {
                glm_vec3_muladds(camera->right, velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_SPACE]) {
                glm_vec3_muladds(camera->up, velocity, movement);
            }
            if (camera->input.keys_pressed[PODI_KEY_SHIFT]) {
                glm_vec3_muladds(camera->up, -velocity, movement);
            }
            break;

        case POC_CAMERA_ORBIT:
            // Orbit camera movement is handled via mouse input
            break;
    }

    // Apply movement
    if (glm_vec3_norm2(movement) > 0.0f) {
        glm_vec3_add(camera->position, movement, camera->position);
        camera->matrices_dirty = true;
    }
}

void poc_camera_process_mouse_movement(poc_camera *camera, double mouse_x, double mouse_y, bool constrain_pitch) {
    if (!camera || !camera->enabled) return;

    if (camera->input.first_mouse) {
        camera->input.last_mouse_x = mouse_x;
        camera->input.last_mouse_y = mouse_y;
        camera->input.first_mouse = false;
    }

    double xoffset = mouse_x - camera->input.last_mouse_x;
    double yoffset = camera->input.last_mouse_y - mouse_y; // Reversed since y-coordinates go from bottom to top

    camera->input.last_mouse_x = mouse_x;
    camera->input.last_mouse_y = mouse_y;

    switch (camera->type) {
        case POC_CAMERA_FIRST_PERSON:
            xoffset *= camera->fps.mouse_sensitivity;
            yoffset *= camera->fps.mouse_sensitivity;

            camera->yaw += (float)xoffset;
            camera->pitch += (float)yoffset;

            if (constrain_pitch || camera->fps.constrain_pitch) {
                if (camera->pitch > 89.0f) camera->pitch = 89.0f;
                if (camera->pitch < -89.0f) camera->pitch = -89.0f;
            }
            break;

        case POC_CAMERA_ORBIT:
            xoffset *= camera->orbit.orbit_speed * 0.1;
            yoffset *= camera->orbit.orbit_speed * 0.1;

            camera->yaw += (float)xoffset;
            camera->pitch += (float)yoffset;

            if (constrain_pitch) {
                if (camera->pitch > 89.0f) camera->pitch = 89.0f;
                if (camera->pitch < -89.0f) camera->pitch = -89.0f;
            }

            // Recalculate position based on target and angles
            float x = camera->orbit.target[0] + camera->orbit.distance * cosf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));
            float y = camera->orbit.target[1] + camera->orbit.distance * sinf(glm_rad(camera->pitch));
            float z = camera->orbit.target[2] + camera->orbit.distance * sinf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));

            camera->position[0] = x;
            camera->position[1] = y;
            camera->position[2] = z;
            break;

        case POC_CAMERA_FREE:
            xoffset *= DEFAULT_SENSITIVITY;
            yoffset *= DEFAULT_SENSITIVITY;

            camera->yaw += (float)xoffset;
            camera->pitch += (float)yoffset;

            if (constrain_pitch) {
                if (camera->pitch > 89.0f) camera->pitch = 89.0f;
                if (camera->pitch < -89.0f) camera->pitch = -89.0f;
            }
            break;
    }

    poc_camera_update_vectors(camera);
    camera->matrices_dirty = true;
}

void poc_camera_process_mouse_scroll(poc_camera *camera, double scroll_y) {
    if (!camera || !camera->enabled) return;

    switch (camera->type) {
        case POC_CAMERA_FIRST_PERSON:
        case POC_CAMERA_FREE:
            {
                float new_fov = camera->fov - (float)scroll_y;
                if (new_fov < 1.0f) new_fov = 1.0f;
                if (new_fov > 120.0f) new_fov = 120.0f;
                poc_camera_set_vertical_fov(camera, new_fov);
            }
            break;

        case POC_CAMERA_ORBIT:
            camera->orbit.distance -= (float)scroll_y * camera->orbit.zoom_speed;
            if (camera->orbit.distance < camera->orbit.min_distance) {
                camera->orbit.distance = camera->orbit.min_distance;
            }
            if (camera->orbit.distance > camera->orbit.max_distance) {
                camera->orbit.distance = camera->orbit.max_distance;
            }

            // Recalculate position
            float x = camera->orbit.target[0] + camera->orbit.distance * cosf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));
            float y = camera->orbit.target[1] + camera->orbit.distance * sinf(glm_rad(camera->pitch));
            float z = camera->orbit.target[2] + camera->orbit.distance * sinf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));

            camera->position[0] = x;
            camera->position[1] = y;
            camera->position[2] = z;

            camera->matrices_dirty = true;
            break;
    }
}

void poc_camera_update(poc_camera *camera, double delta_time) {
    if (!camera || !camera->enabled) return;

    camera->input.delta_time = delta_time;
    poc_camera_update_matrices(camera);
}

const float *poc_camera_get_front(poc_camera *camera) {
    return camera ? (const float *)camera->front : NULL;
}

const float *poc_camera_get_up(poc_camera *camera) {
    return camera ? (const float *)camera->up : NULL;
}

const float *poc_camera_get_right(poc_camera *camera) {
    return camera ? (const float *)camera->right : NULL;
}

// Internal helper functions

void poc_camera_update_vectors(poc_camera *camera) {
    if (!camera) return;

    if (camera->type == POC_CAMERA_ORBIT) {
        // For orbit camera, front points toward the target
        glm_vec3_sub(camera->orbit.target, camera->position, camera->front);
        glm_vec3_normalize(camera->front);
    } else {
        // Calculate front vector from Euler angles
        vec3 front;
        front[0] = cosf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));
        front[1] = sinf(glm_rad(camera->pitch));
        front[2] = sinf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));
        glm_vec3_normalize(front);
        glm_vec3_copy(front, camera->front);
    }

    // Calculate right and up vectors
    glm_vec3_cross(camera->front, camera->world_up, camera->right);
    glm_vec3_normalize(camera->right);

    glm_vec3_cross(camera->right, camera->front, camera->up);
    glm_vec3_normalize(camera->up);
}

static void calculate_view_matrix(poc_camera *camera) {
    if (!camera) return;

    vec3 center;
    glm_vec3_add(camera->position, camera->front, center);
    glm_lookat(camera->position, center, camera->up, camera->view_matrix);
}

static void calculate_projection_matrix(poc_camera *camera) {
    if (!camera) return;

    glm_perspective(glm_rad(camera->fov), camera->aspect_ratio,
                   camera->near_plane, camera->far_plane, camera->projection_matrix);
}

bool poc_camera_screen_to_ray(const poc_camera *camera, float screen_x, float screen_y, poc_ray *ray) {
    if (!camera || !ray) {
        return false;
    }

    // Ensure matrices are up to date
    poc_camera *mutable_camera = (poc_camera*)camera;
    poc_camera_update_matrices(mutable_camera);

    // Convert screen coordinates to NDC (Normalized Device Coordinates)
    // Screen coords are [0,1] where (0,0) is top-left
    // NDC is [-1,1] where (-1,-1) is bottom-left
    float ndc_x = (screen_x * 2.0f) - 1.0f;
    float ndc_y = 1.0f - (screen_y * 2.0f);  // Flip Y axis

    // Create clip space coordinates for near and far plane
    vec4 near_clip = {ndc_x, ndc_y, -1.0f, 1.0f};  // Near plane
    vec4 far_clip = {ndc_x, ndc_y, 1.0f, 1.0f};    // Far plane

    // Calculate inverse view-projection matrix
    mat4 view_proj;
    mat4 inv_view_proj;
    mat4 proj_copy, view_copy;

    // Copy const matrices to local copies
    memcpy(proj_copy, camera->projection_matrix, sizeof(mat4));
    memcpy(view_copy, camera->view_matrix, sizeof(mat4));

    glm_mat4_mul(proj_copy, view_copy, view_proj);

    glm_mat4_inv(view_proj, inv_view_proj);

    // Unproject near and far points to world space
    vec4 near_world, far_world;
    glm_mat4_mulv(inv_view_proj, near_clip, near_world);
    glm_mat4_mulv(inv_view_proj, far_clip, far_world);

    // Perspective divide
    if (near_world[3] == 0.0f || far_world[3] == 0.0f) {
        return false; // Invalid homogeneous coordinates
    }

    vec3 near_point = {
        near_world[0] / near_world[3],
        near_world[1] / near_world[3],
        near_world[2] / near_world[3]
    };

    vec3 far_point = {
        far_world[0] / far_world[3],
        far_world[1] / far_world[3],
        far_world[2] / far_world[3]
    };

    // Ray origin is the camera position
    vec3 pos_copy;
    memcpy(pos_copy, camera->position, sizeof(vec3));
    glm_vec3_copy(pos_copy, ray->origin);

    // Ray direction is from near point towards far point
    glm_vec3_sub(far_point, near_point, ray->direction);
    glm_vec3_normalize(ray->direction);

    return true;
}
