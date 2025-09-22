#include "camera.h"
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
    camera.fov = DEFAULT_ZOOM;
    camera.aspect_ratio = aspect_ratio;
    camera.near_plane = DEFAULT_NEAR;
    camera.far_plane = DEFAULT_FAR;

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
    if (!camera) return;

    camera->fov = fov;
    camera->matrices_dirty = true;
}

void poc_camera_set_aspect_ratio(poc_camera *camera, float aspect_ratio) {
    if (!camera) return;

    camera->aspect_ratio = aspect_ratio;
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
            camera->fov -= (float)scroll_y;
            if (camera->fov < 1.0f) camera->fov = 1.0f;
            if (camera->fov > 120.0f) camera->fov = 120.0f;
            camera->matrices_dirty = true;
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