/**
 * @file math_bindings.h
 * @brief Mathematical operations Lua bindings for POC Engine
 *
 * This module provides Lua bindings for cglm mathematical operations,
 * including vec3, mat4, and transform utilities. It ensures consistent
 * access to mathematical operations between C and Lua contexts.
 */

#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <cglm/cglm.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Metatable names for mathematical userdata types
#define VEC3_MATH_METATABLE "POCEngine.Vec3Math"
#define MAT4_MATH_METATABLE "POCEngine.Mat4Math"
#define TRANSFORM_METATABLE "POCEngine.Transform"

/**
 * @brief Vec3 userdata structure for mathematical operations
 *
 * This structure represents a standalone vec3 that owns its data,
 * unlike the camera position vec3 which references existing memory.
 */
typedef struct {
    vec3 data;          /**< The actual vector data */
    bool owns_data;     /**< Whether this userdata owns the data */
} vec3_math_userdata;

/**
 * @brief Mat4 userdata structure for matrix operations
 */
typedef struct {
    mat4 data;          /**< The actual matrix data */
    bool owns_data;     /**< Whether this userdata owns the data */
} mat4_math_userdata;

/**
 * @brief Transform userdata structure for high-level transforms
 */
typedef struct {
    vec3 position;      /**< Position component */
    vec3 rotation;      /**< Rotation as Euler angles (degrees) */
    vec3 scale;         /**< Scale component */
    mat4 matrix;        /**< Cached transformation matrix */
    bool matrix_dirty;  /**< Whether matrix needs recalculation */
} transform_userdata;

/**
 * @brief Register all mathematical Lua bindings
 *
 * Registers vec3, mat4, and transform userdata types with their
 * metamethods and utility functions. Call this during scripting
 * system initialization.
 *
 * @param L The Lua state to register bindings in
 */
void poc_math_register_bindings(lua_State *L);

/**
 * @brief Create a new vec3 userdata and push it to Lua stack
 *
 * @param L The Lua state
 * @param x X component
 * @param y Y component
 * @param z Z component
 * @return 1 (number of values pushed to stack)
 */
int poc_math_push_vec3(lua_State *L, float x, float y, float z);

/**
 * @brief Create a vec3 userdata from existing vec3 data
 *
 * @param L The Lua state
 * @param vec Source vector data
 * @param owns_data Whether the userdata should own/copy the data
 * @return 1 (number of values pushed to stack)
 */
int poc_math_push_vec3_from_data(lua_State *L, const vec3 vec, bool owns_data);

/**
 * @brief Create a new mat4 userdata and push it to Lua stack
 *
 * @param L The Lua state
 * @param mat Source matrix data (if NULL, creates identity)
 * @return 1 (number of values pushed to stack)
 */
int poc_math_push_mat4(lua_State *L, const mat4 mat);

/**
 * @brief Check and retrieve vec3 userdata from Lua stack
 *
 * @param L The Lua state
 * @param index Stack index
 * @return Pointer to vec3_math_userdata, or NULL if invalid
 */
vec3_math_userdata *poc_math_check_vec3(lua_State *L, int index);

/**
 * @brief Check and retrieve mat4 userdata from Lua stack
 *
 * @param L The Lua state
 * @param index Stack index
 * @return Pointer to mat4_math_userdata, or NULL if invalid
 */
mat4_math_userdata *poc_math_check_mat4(lua_State *L, int index);

/**
 * @brief Check and retrieve transform userdata from Lua stack
 *
 * @param L The Lua state
 * @param index Stack index
 * @return Pointer to transform_userdata, or NULL if invalid
 */
transform_userdata *poc_math_check_transform(lua_State *L, int index);

#ifdef __cplusplus
}
#endif