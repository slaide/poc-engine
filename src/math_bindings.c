#include "math_bindings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declarations for metamethods
static int lua_vec3_math_index(lua_State *L);
static int lua_vec3_math_newindex(lua_State *L);
static int lua_vec3_math_add(lua_State *L);
static int lua_vec3_math_sub(lua_State *L);
static int lua_vec3_math_mul(lua_State *L);
static int lua_vec3_math_unm(lua_State *L);
static int lua_vec3_math_tostring(lua_State *L);
static int lua_vec3_math_gc(lua_State *L);

static int lua_mat4_math_index(lua_State *L);
static int lua_mat4_math_newindex(lua_State *L);
static int lua_mat4_math_mul(lua_State *L);
static int lua_mat4_math_tostring(lua_State *L);
static int lua_mat4_math_gc(lua_State *L);

static int lua_transform_index(lua_State *L);
static int lua_transform_newindex(lua_State *L);
static int lua_transform_tostring(lua_State *L);
static int lua_transform_gc(lua_State *L);

// Constructor functions
static int lua_vec3_new(lua_State *L);
static int lua_mat4_new(lua_State *L);
static int lua_mat4_identity(lua_State *L);
static int lua_transform_new(lua_State *L);

// Vec3 method functions
static int lua_vec3_normalize(lua_State *L);
static int lua_vec3_length(lua_State *L);
static int lua_vec3_dot(lua_State *L);
static int lua_vec3_cross(lua_State *L);
static int lua_vec3_distance(lua_State *L);
static int lua_vec3_lerp(lua_State *L);
static int lua_vec3_scale(lua_State *L);

// Mat4 method functions
static int lua_mat4_translate(lua_State *L);
static int lua_mat4_rotate(lua_State *L);
static int lua_mat4_scale_mat(lua_State *L);
static int lua_mat4_inverse(lua_State *L);
static int lua_mat4_transpose(lua_State *L);
static int lua_mat4_forward(lua_State *L);
static int lua_mat4_right(lua_State *L);
static int lua_mat4_up(lua_State *L);

// Transform method functions
static int lua_transform_get_matrix(lua_State *L);
static int lua_transform_get_forward(lua_State *L);
static int lua_transform_get_right(lua_State *L);
static int lua_transform_get_up(lua_State *L);

// Helper functions
static void update_transform_matrix(transform_userdata *transform);

void poc_math_register_bindings(lua_State *L) {
    if (!L) return;

    // Create Vec3 metatable
    luaL_newmetatable(L, VEC3_MATH_METATABLE);
    lua_pushcfunction(L, lua_vec3_math_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_vec3_math_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, lua_vec3_math_add);
    lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, lua_vec3_math_sub);
    lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, lua_vec3_math_mul);
    lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, lua_vec3_math_unm);
    lua_setfield(L, -2, "__unm");
    lua_pushcfunction(L, lua_vec3_math_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, lua_vec3_math_gc);
    lua_setfield(L, -2, "__gc");

    // Vec3 methods table
    lua_newtable(L);
    lua_pushcfunction(L, lua_vec3_normalize);
    lua_setfield(L, -2, "normalize");
    lua_pushcfunction(L, lua_vec3_length);
    lua_setfield(L, -2, "length");
    lua_pushcfunction(L, lua_vec3_dot);
    lua_setfield(L, -2, "dot");
    lua_pushcfunction(L, lua_vec3_cross);
    lua_setfield(L, -2, "cross");
    lua_pushcfunction(L, lua_vec3_distance);
    lua_setfield(L, -2, "distance");
    lua_pushcfunction(L, lua_vec3_lerp);
    lua_setfield(L, -2, "lerp");
    lua_pushcfunction(L, lua_vec3_scale);
    lua_setfield(L, -2, "scale");
    lua_setfield(L, -2, "__methods");

    lua_pop(L, 1); // Remove metatable from stack

    // Create Mat4 metatable
    luaL_newmetatable(L, MAT4_MATH_METATABLE);
    lua_pushcfunction(L, lua_mat4_math_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_mat4_math_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, lua_mat4_math_mul);
    lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, lua_mat4_math_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, lua_mat4_math_gc);
    lua_setfield(L, -2, "__gc");

    // Mat4 methods table
    lua_newtable(L);
    lua_pushcfunction(L, lua_mat4_translate);
    lua_setfield(L, -2, "translate");
    lua_pushcfunction(L, lua_mat4_rotate);
    lua_setfield(L, -2, "rotate");
    lua_pushcfunction(L, lua_mat4_scale_mat);
    lua_setfield(L, -2, "scale");
    lua_pushcfunction(L, lua_mat4_inverse);
    lua_setfield(L, -2, "inverse");
    lua_pushcfunction(L, lua_mat4_transpose);
    lua_setfield(L, -2, "transpose");
    lua_pushcfunction(L, lua_mat4_forward);
    lua_setfield(L, -2, "forward");
    lua_pushcfunction(L, lua_mat4_right);
    lua_setfield(L, -2, "right");
    lua_pushcfunction(L, lua_mat4_up);
    lua_setfield(L, -2, "up");
    lua_setfield(L, -2, "__methods");

    lua_pop(L, 1); // Remove metatable from stack

    // Create Transform metatable
    luaL_newmetatable(L, TRANSFORM_METATABLE);
    lua_pushcfunction(L, lua_transform_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_transform_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, lua_transform_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, lua_transform_gc);
    lua_setfield(L, -2, "__gc");

    // Transform methods table
    lua_newtable(L);
    lua_pushcfunction(L, lua_transform_get_matrix);
    lua_setfield(L, -2, "get_matrix");
    lua_pushcfunction(L, lua_transform_get_forward);
    lua_setfield(L, -2, "get_forward");
    lua_pushcfunction(L, lua_transform_get_right);
    lua_setfield(L, -2, "get_right");
    lua_pushcfunction(L, lua_transform_get_up);
    lua_setfield(L, -2, "get_up");
    lua_setfield(L, -2, "__methods");

    lua_pop(L, 1); // Remove metatable from stack

    // Create Math table with constructor functions
    lua_newtable(L);

    // Vec3 constructor
    lua_pushcfunction(L, lua_vec3_new);
    lua_setfield(L, -2, "vec3");

    // Mat4 constructors
    lua_newtable(L);
    lua_pushcfunction(L, lua_mat4_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, lua_mat4_identity);
    lua_setfield(L, -2, "identity");
    lua_setfield(L, -2, "mat4");

    // Transform constructor
    lua_pushcfunction(L, lua_transform_new);
    lua_setfield(L, -2, "transform");

    // Set Math table as global
    lua_setglobal(L, "Math");

    printf("✓ POC Engine math bindings registered\n");
}

// Constructor implementations

static int lua_vec3_new(lua_State *L) {
    float x = (float)luaL_optnumber(L, 1, 0.0);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 0.0);

    return poc_math_push_vec3(L, x, y, z);
}

static int lua_mat4_new(lua_State *L) {
    return poc_math_push_mat4(L, NULL);
}

static int lua_mat4_identity(lua_State *L) {
    mat4 identity;
    glm_mat4_identity(identity);
    return poc_math_push_mat4(L, identity);
}

static int lua_transform_new(lua_State *L) {
    transform_userdata *transform = (transform_userdata *)lua_newuserdata(L, sizeof(transform_userdata));

    // Initialize with defaults
    glm_vec3_zero(transform->position);
    glm_vec3_zero(transform->rotation);
    glm_vec3_one(transform->scale);
    glm_mat4_identity(transform->matrix);
    transform->matrix_dirty = true;

    luaL_getmetatable(L, TRANSFORM_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// Helper functions

int poc_math_push_vec3(lua_State *L, float x, float y, float z) {
    vec3_math_userdata *userdata = (vec3_math_userdata *)lua_newuserdata(L, sizeof(vec3_math_userdata));
    userdata->data[0] = x;
    userdata->data[1] = y;
    userdata->data[2] = z;
    userdata->owns_data = true;

    luaL_getmetatable(L, VEC3_MATH_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int poc_math_push_vec3_from_data(lua_State *L, const vec3 vec, bool owns_data) {
    vec3_math_userdata *userdata = (vec3_math_userdata *)lua_newuserdata(L, sizeof(vec3_math_userdata));
    userdata->data[0] = vec[0];
    userdata->data[1] = vec[1];
    userdata->data[2] = vec[2];
    userdata->owns_data = owns_data;

    luaL_getmetatable(L, VEC3_MATH_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int poc_math_push_mat4(lua_State *L, const mat4 mat) {
    mat4_math_userdata *userdata = (mat4_math_userdata *)lua_newuserdata(L, sizeof(mat4_math_userdata));

    if (mat) {
        memcpy(userdata->data, mat, sizeof(mat4));
    } else {
        glm_mat4_identity(userdata->data);
    }
    userdata->owns_data = true;

    luaL_getmetatable(L, MAT4_MATH_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

vec3_math_userdata *poc_math_check_vec3(lua_State *L, int index) {
    return (vec3_math_userdata *)luaL_checkudata(L, index, VEC3_MATH_METATABLE);
}

mat4_math_userdata *poc_math_check_mat4(lua_State *L, int index) {
    return (mat4_math_userdata *)luaL_checkudata(L, index, MAT4_MATH_METATABLE);
}

transform_userdata *poc_math_check_transform(lua_State *L, int index) {
    return (transform_userdata *)luaL_checkudata(L, index, TRANSFORM_METATABLE);
}

// Vec3 metamethod implementations

static int lua_vec3_math_index(lua_State *L) {
    vec3_math_userdata *userdata = poc_math_check_vec3(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (!userdata || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, userdata->data[0]);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, userdata->data[1]);
        return 1;
    } else if (strcmp(key, "z") == 0) {
        lua_pushnumber(L, userdata->data[2]);
        return 1;
    } else {
        // Check methods table
        luaL_getmetatable(L, VEC3_MATH_METATABLE);
        lua_getfield(L, -1, "__methods");
        lua_getfield(L, -1, key);
        return 1;
    }
}

static int lua_vec3_math_newindex(lua_State *L) {
    vec3_math_userdata *userdata = poc_math_check_vec3(L, 1);
    const char *key = luaL_checkstring(L, 2);
    float value = (float)luaL_checknumber(L, 3);

    if (!userdata || !key) return 0;

    if (strcmp(key, "x") == 0) {
        userdata->data[0] = value;
    } else if (strcmp(key, "y") == 0) {
        userdata->data[1] = value;
    } else if (strcmp(key, "z") == 0) {
        userdata->data[2] = value;
    } else {
        luaL_error(L, "Invalid vec3 property '%s'", key);
    }

    return 0;
}

static int lua_vec3_math_add(lua_State *L) {
    vec3_math_userdata *a = poc_math_check_vec3(L, 1);
    vec3_math_userdata *b = poc_math_check_vec3(L, 2);

    if (!a || !b) {
        luaL_error(L, "Cannot add non-vec3 values");
        return 0;
    }

    vec3 result;
    glm_vec3_add(a->data, b->data, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_math_sub(lua_State *L) {
    vec3_math_userdata *a = poc_math_check_vec3(L, 1);
    vec3_math_userdata *b = poc_math_check_vec3(L, 2);

    if (!a || !b) {
        luaL_error(L, "Cannot subtract non-vec3 values");
        return 0;
    }

    vec3 result;
    glm_vec3_sub(a->data, b->data, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_math_mul(lua_State *L) {
    vec3_math_userdata *vec = poc_math_check_vec3(L, 1);
    float scalar = (float)luaL_checknumber(L, 2);

    if (!vec) {
        luaL_error(L, "Cannot multiply non-vec3 value");
        return 0;
    }

    vec3 result;
    glm_vec3_scale(vec->data, scalar, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_math_unm(lua_State *L) {
    vec3_math_userdata *vec = poc_math_check_vec3(L, 1);

    if (!vec) {
        luaL_error(L, "Cannot negate non-vec3 value");
        return 0;
    }

    vec3 result;
    glm_vec3_negate_to(vec->data, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_math_tostring(lua_State *L) {
    vec3_math_userdata *vec = poc_math_check_vec3(L, 1);
    if (!vec) return 0;

    lua_pushfstring(L, "vec3(%.3f, %.3f, %.3f)", vec->data[0], vec->data[1], vec->data[2]);
    return 1;
}

static int lua_vec3_math_gc(lua_State *L) {
    (void)L; // Unused parameter
    // No additional cleanup needed for vec3
    return 0;
}

// Vec3 method implementations

static int lua_vec3_normalize(lua_State *L) {
    vec3_math_userdata *vec = poc_math_check_vec3(L, 1);
    if (!vec) return 0;

    vec3 result;
    glm_vec3_normalize_to(vec->data, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_length(lua_State *L) {
    vec3_math_userdata *vec = poc_math_check_vec3(L, 1);
    if (!vec) return 0;

    float length = glm_vec3_norm(vec->data);
    lua_pushnumber(L, length);
    return 1;
}

static int lua_vec3_dot(lua_State *L) {
    vec3_math_userdata *a = poc_math_check_vec3(L, 1);
    vec3_math_userdata *b = poc_math_check_vec3(L, 2);

    if (!a || !b) {
        luaL_error(L, "vec3:dot requires another vec3");
        return 0;
    }

    float dot = glm_vec3_dot(a->data, b->data);
    lua_pushnumber(L, dot);
    return 1;
}

static int lua_vec3_cross(lua_State *L) {
    vec3_math_userdata *a = poc_math_check_vec3(L, 1);
    vec3_math_userdata *b = poc_math_check_vec3(L, 2);

    if (!a || !b) {
        luaL_error(L, "vec3:cross requires another vec3");
        return 0;
    }

    vec3 result;
    glm_vec3_cross(a->data, b->data, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_distance(lua_State *L) {
    vec3_math_userdata *a = poc_math_check_vec3(L, 1);
    vec3_math_userdata *b = poc_math_check_vec3(L, 2);

    if (!a || !b) {
        luaL_error(L, "vec3:distance requires another vec3");
        return 0;
    }

    float distance = glm_vec3_distance(a->data, b->data);
    lua_pushnumber(L, distance);
    return 1;
}

static int lua_vec3_lerp(lua_State *L) {
    vec3_math_userdata *a = poc_math_check_vec3(L, 1);
    vec3_math_userdata *b = poc_math_check_vec3(L, 2);
    float t = (float)luaL_checknumber(L, 3);

    if (!a || !b) {
        luaL_error(L, "vec3:lerp requires another vec3 and a number");
        return 0;
    }

    vec3 result;
    glm_vec3_lerp(a->data, b->data, t, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

static int lua_vec3_scale(lua_State *L) {
    vec3_math_userdata *vec = poc_math_check_vec3(L, 1);
    float scalar = (float)luaL_checknumber(L, 2);

    if (!vec) {
        luaL_error(L, "vec3:scale requires a number");
        return 0;
    }

    vec3 result;
    glm_vec3_scale(vec->data, scalar, result);
    return poc_math_push_vec3_from_data(L, result, true);
}

// Mat4 metamethod implementations (basic stubs for now)

static int lua_mat4_math_index(lua_State *L) {
    const char *key = luaL_checkstring(L, 2);

    // Check methods table
    luaL_getmetatable(L, MAT4_MATH_METATABLE);
    lua_getfield(L, -1, "__methods");
    lua_getfield(L, -1, key);
    return 1;
}

static int lua_mat4_math_newindex(lua_State *L) {
    luaL_error(L, "mat4 components are read-only");
    return 0;
}

static int lua_mat4_math_mul(lua_State *L) {
    mat4_math_userdata *a = poc_math_check_mat4(L, 1);
    mat4_math_userdata *b = poc_math_check_mat4(L, 2);

    if (!a || !b) {
        luaL_error(L, "Cannot multiply non-mat4 values");
        return 0;
    }

    mat4 result;
    glm_mat4_mul(a->data, b->data, result);
    return poc_math_push_mat4(L, result);
}

static int lua_mat4_math_tostring(lua_State *L) {
    lua_pushstring(L, "mat4");
    return 1;
}

static int lua_mat4_math_gc(lua_State *L) {
    (void)L; // Unused parameter
    return 0;
}

// Mat4 method stubs (to be implemented as needed)

static int lua_mat4_translate(lua_State *L) {
    luaL_error(L, "mat4:translate not yet implemented");
    return 0;
}

static int lua_mat4_rotate(lua_State *L) {
    luaL_error(L, "mat4:rotate not yet implemented");
    return 0;
}

static int lua_mat4_scale_mat(lua_State *L) {
    luaL_error(L, "mat4:scale not yet implemented");
    return 0;
}

static int lua_mat4_inverse(lua_State *L) {
    luaL_error(L, "mat4:inverse not yet implemented");
    return 0;
}

static int lua_mat4_transpose(lua_State *L) {
    luaL_error(L, "mat4:transpose not yet implemented");
    return 0;
}

static int lua_mat4_forward(lua_State *L) {
    mat4_math_userdata *mat = poc_math_check_mat4(L, 1);
    if (!mat) return 0;

    // Extract forward vector from matrix (negative Z column)
    vec3 forward = {-mat->data[2][0], -mat->data[2][1], -mat->data[2][2]};
    return poc_math_push_vec3_from_data(L, forward, true);
}

static int lua_mat4_right(lua_State *L) {
    mat4_math_userdata *mat = poc_math_check_mat4(L, 1);
    if (!mat) return 0;

    // Extract right vector from matrix (X column)
    vec3 right = {mat->data[0][0], mat->data[0][1], mat->data[0][2]};
    return poc_math_push_vec3_from_data(L, right, true);
}

static int lua_mat4_up(lua_State *L) {
    mat4_math_userdata *mat = poc_math_check_mat4(L, 1);
    if (!mat) return 0;

    // Extract up vector from matrix (Y column)
    vec3 up = {mat->data[1][0], mat->data[1][1], mat->data[1][2]};
    return poc_math_push_vec3_from_data(L, up, true);
}

// Transform implementations (basic stubs)

static int lua_transform_index(lua_State *L) {
    transform_userdata *transform = poc_math_check_transform(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (!transform || !key) return 0;

    if (strcmp(key, "position") == 0) {
        return poc_math_push_vec3_from_data(L, transform->position, false);
    } else if (strcmp(key, "rotation") == 0) {
        return poc_math_push_vec3_from_data(L, transform->rotation, false);
    } else if (strcmp(key, "scale") == 0) {
        return poc_math_push_vec3_from_data(L, transform->scale, false);
    } else {
        // Check methods table
        luaL_getmetatable(L, TRANSFORM_METATABLE);
        lua_getfield(L, -1, "__methods");
        lua_getfield(L, -1, key);
        return 1;
    }
}

static int lua_transform_newindex(lua_State *L) {
    luaL_error(L, "Transform properties should be modified through their vec3 components");
    return 0;
}

static int lua_transform_tostring(lua_State *L) {
    lua_pushstring(L, "transform");
    return 1;
}

static int lua_transform_gc(lua_State *L) {
    (void)L; // Unused parameter
    return 0;
}

// Transform method stubs

static int lua_transform_get_matrix(lua_State *L) {
    transform_userdata *transform = poc_math_check_transform(L, 1);
    if (!transform) return 0;

    if (transform->matrix_dirty) {
        update_transform_matrix(transform);
    }

    return poc_math_push_mat4(L, transform->matrix);
}

static int lua_transform_get_forward(lua_State *L) {
    luaL_error(L, "transform:get_forward not yet implemented");
    return 0;
}

static int lua_transform_get_right(lua_State *L) {
    luaL_error(L, "transform:get_right not yet implemented");
    return 0;
}

static int lua_transform_get_up(lua_State *L) {
    luaL_error(L, "transform:get_up not yet implemented");
    return 0;
}

static void update_transform_matrix(transform_userdata *transform) {
    // Create transformation matrix from position, rotation, scale
    mat4 translation, rotation_x, rotation_y, rotation_z, scale, temp;
    mat4 identity;

    // Translation
    glm_translate_make(translation, transform->position);

    // Rotation (order: Z, Y, X)
    glm_mat4_identity(identity);
    glm_rotate_z(identity, glm_rad(transform->rotation[2]), rotation_z);
    glm_mat4_identity(identity);
    glm_rotate_y(identity, glm_rad(transform->rotation[1]), rotation_y);
    glm_mat4_identity(identity);
    glm_rotate_x(identity, glm_rad(transform->rotation[0]), rotation_x);

    // Scale
    glm_scale_make(scale, transform->scale);

    // Combine: T * R * S
    glm_mat4_mul(rotation_y, rotation_x, temp);
    glm_mat4_mul(rotation_z, temp, temp);
    glm_mat4_mul(temp, scale, temp);
    glm_mat4_mul(translation, temp, transform->matrix);

    transform->matrix_dirty = false;
}