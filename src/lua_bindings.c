#include "scripting.h"
#include "camera.h"
#include "poc_engine.h"
#include "vulkan_renderer.h"
#include "math_bindings.h"
#include <podi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Metatable names for userdata
#define CAMERA_METATABLE "POCEngine.Camera"
#define VEC3_METATABLE "POCEngine.Vec3"

// Forward declarations for binding functions
static int lua_poc_get_time(lua_State *L);
static int lua_poc_sleep(lua_State *L);
static int lua_poc_create_camera(lua_State *L);
static int lua_poc_bind_camera(lua_State *L);
static int lua_poc_quit_application(lua_State *L);

// Camera userdata methods
static int lua_camera_update(lua_State *L);
static int lua_camera_process_keyboard(lua_State *L);
static int lua_camera_process_mouse_movement(lua_State *L);
static int lua_camera_process_mouse_scroll(lua_State *L);
static int lua_camera_index(lua_State *L);
static int lua_camera_newindex(lua_State *L);
static int lua_camera_gc(lua_State *L);

// Vec3 userdata for position properties
static int lua_vec3_index(lua_State *L);
static int lua_vec3_newindex(lua_State *L);
static int lua_vec3_gc(lua_State *L);

// Utility functions
static poc_camera *check_camera(lua_State *L, int index);
static poc_camera *push_camera_userdata(lua_State *L, poc_camera *camera);
static void push_vec3_userdata(lua_State *L, vec3 *v, poc_camera *camera, const char *property);

// Global context for camera binding
static struct poc_context *g_active_context = NULL;

// Global window for application control
static podi_window *g_active_window = NULL;

void poc_scripting_register_bindings(lua_State *L) {
    if (!L) return;

    // Create Camera metatable
    luaL_newmetatable(L, CAMERA_METATABLE);
    lua_pushcfunction(L, lua_camera_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_camera_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, lua_camera_gc);
    lua_setfield(L, -2, "__gc");

    // Camera methods table
    lua_newtable(L);
    lua_pushcfunction(L, lua_camera_update);
    lua_setfield(L, -2, "update");
    lua_pushcfunction(L, lua_camera_process_keyboard);
    lua_setfield(L, -2, "process_keyboard");
    lua_pushcfunction(L, lua_camera_process_mouse_movement);
    lua_setfield(L, -2, "process_mouse_movement");
    lua_pushcfunction(L, lua_camera_process_mouse_scroll);
    lua_setfield(L, -2, "process_mouse_scroll");
    lua_setfield(L, -2, "__methods");

    lua_pop(L, 1); // Remove metatable from stack

    // Create Vec3 metatable
    luaL_newmetatable(L, VEC3_METATABLE);
    lua_pushcfunction(L, lua_vec3_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_vec3_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, lua_vec3_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1); // Remove metatable from stack

    // Create POC table
    lua_newtable(L);

    // Register POC Engine functions
    lua_pushcfunction(L, lua_poc_get_time);
    lua_setfield(L, -2, "get_time");

    lua_pushcfunction(L, lua_poc_sleep);
    lua_setfield(L, -2, "sleep");

    // Camera creation and management
    lua_pushcfunction(L, lua_poc_create_camera);
    lua_setfield(L, -2, "create_camera");

    lua_pushcfunction(L, lua_poc_bind_camera);
    lua_setfield(L, -2, "bind_camera");

    // Application control
    lua_pushcfunction(L, lua_poc_quit_application);
    lua_setfield(L, -2, "quit_application");

    // Set POC table as global
    lua_setglobal(L, "POC");

    // Register camera type constants
    lua_newtable(L);
    lua_pushinteger(L, POC_CAMERA_FIRST_PERSON);
    lua_setfield(L, -2, "FIRST_PERSON");
    lua_pushinteger(L, POC_CAMERA_ORBIT);
    lua_setfield(L, -2, "ORBIT");
    lua_pushinteger(L, POC_CAMERA_FREE);
    lua_setfield(L, -2, "FREE");
    lua_setglobal(L, "CAMERA_TYPE");

    // Register key constants
    lua_newtable(L);
    lua_pushinteger(L, PODI_KEY_W);
    lua_setfield(L, -2, "W");
    lua_pushinteger(L, PODI_KEY_A);
    lua_setfield(L, -2, "A");
    lua_pushinteger(L, PODI_KEY_S);
    lua_setfield(L, -2, "S");
    lua_pushinteger(L, PODI_KEY_D);
    lua_setfield(L, -2, "D");
    lua_pushinteger(L, PODI_KEY_SPACE);
    lua_setfield(L, -2, "SPACE");
    lua_pushinteger(L, PODI_KEY_SHIFT);
    lua_setfield(L, -2, "SHIFT");
    lua_pushinteger(L, PODI_KEY_ESCAPE);
    lua_setfield(L, -2, "ESCAPE");
    lua_setglobal(L, "KEY");

    printf("✓ POC Engine Lua userdata bindings registered\n");
}

// POC Engine function bindings

static int lua_poc_get_time(lua_State *L) {
    double time = poc_get_time();
    lua_pushnumber(L, time);
    return 1;
}

static int lua_poc_sleep(lua_State *L) {
    double seconds = luaL_checknumber(L, 1);
    poc_sleep(seconds);
    return 0;
}

static int lua_poc_create_camera(lua_State *L) {
    const char *type_str = luaL_checkstring(L, 1);
    double aspect_ratio = luaL_checknumber(L, 2);

    poc_camera_type type;
    if (strcmp(type_str, "fps") == 0) {
        type = POC_CAMERA_FIRST_PERSON;
    } else if (strcmp(type_str, "orbit") == 0) {
        type = POC_CAMERA_ORBIT;
    } else if (strcmp(type_str, "free") == 0) {
        type = POC_CAMERA_FREE;
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid camera type. Use 'fps', 'orbit', or 'free'");
        return 2;
    }

    // Allocate camera on heap (will be managed by Lua's GC)
    poc_camera *camera = malloc(sizeof(poc_camera));
    if (!camera) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to allocate camera");
        return 2;
    }

    *camera = poc_camera_create(type, (float)aspect_ratio);
    return push_camera_userdata(L, camera) ? 1 : 0;
}

static int lua_poc_bind_camera(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    if (!camera) {
        lua_pushstring(L, "Invalid camera object");
        lua_error(L);
        return 0;
    }

    if (!g_active_context) {
        lua_pushstring(L, "No active rendering context set");
        lua_error(L);
        return 0;
    }

    // Set camera on the active context
    vulkan_context_set_camera(g_active_context, camera);

    return 0;
}

static int lua_poc_quit_application(lua_State *L) {
    if (!g_active_window) {
        lua_pushstring(L, "No active window set - cannot quit application");
        lua_error(L);
        return 0;
    }

    // Request window closure
    podi_window_close(g_active_window);
    return 0;
}

// Set the context for camera binding
void poc_scripting_set_context(struct poc_context *context) {
    g_active_context = context;
}

// Set the window for application control
void poc_scripting_set_window(podi_window *window) {
    g_active_window = window;
}

// Camera userdata implementation

static poc_camera *check_camera(lua_State *L, int index) {
    return (poc_camera *)luaL_checkudata(L, index, CAMERA_METATABLE);
}

static poc_camera *push_camera_userdata(lua_State *L, poc_camera *camera) {
    if (!camera) return NULL;

    poc_camera *userdata = (poc_camera *)lua_newuserdata(L, sizeof(poc_camera));
    *userdata = *camera;

    luaL_getmetatable(L, CAMERA_METATABLE);
    lua_setmetatable(L, -2);

    return userdata;
}

static int lua_camera_index(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (!camera || !key) return 0;

    if (strcmp(key, "position") == 0) {
        push_vec3_userdata(L, &camera->position, camera, "position");
        return 1;
    } else if (strcmp(key, "front") == 0) {
        poc_math_push_vec3_from_data(L, camera->front, false);
        return 1;
    } else if (strcmp(key, "right") == 0) {
        poc_math_push_vec3_from_data(L, camera->right, false);
        return 1;
    } else if (strcmp(key, "up") == 0) {
        poc_math_push_vec3_from_data(L, camera->up, false);
        return 1;
    } else if (strcmp(key, "yaw") == 0) {
        lua_pushnumber(L, camera->yaw);
        return 1;
    } else if (strcmp(key, "pitch") == 0) {
        lua_pushnumber(L, camera->pitch);
        return 1;
    } else if (strcmp(key, "roll") == 0) {
        lua_pushnumber(L, camera->roll);
        return 1;
    } else if (strcmp(key, "fov") == 0) {
        lua_pushnumber(L, camera->fov);
        return 1;
    } else if (strcmp(key, "type") == 0) {
        lua_pushinteger(L, camera->type);
        return 1;
    } else {
        // Check methods table
        luaL_getmetatable(L, CAMERA_METATABLE);
        lua_getfield(L, -1, "__methods");
        lua_getfield(L, -1, key);
        return 1;
    }
}

static int lua_camera_newindex(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (!camera || !key) return 0;

    if (strcmp(key, "yaw") == 0) {
        camera->yaw = (float)luaL_checknumber(L, 3);
        camera->matrices_dirty = true;
    } else if (strcmp(key, "pitch") == 0) {
        camera->pitch = (float)luaL_checknumber(L, 3);
        camera->matrices_dirty = true;
    } else if (strcmp(key, "roll") == 0) {
        camera->roll = (float)luaL_checknumber(L, 3);
        camera->matrices_dirty = true;
    } else if (strcmp(key, "fov") == 0) {
        camera->fov = (float)luaL_checknumber(L, 3);
        camera->matrices_dirty = true;
    } else {
        luaL_error(L, "Cannot set property '%s' on camera", key);
    }

    return 0;
}

static int lua_camera_gc(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    if (camera) {
        // Camera memory is managed by Lua's garbage collector
        // No additional cleanup needed for basic camera struct
    }
    return 0;
}

static int lua_camera_update(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    double delta_time = luaL_checknumber(L, 2);

    if (!camera) {
        lua_pushstring(L, "Invalid camera object");
        lua_error(L);
        return 0;
    }

    poc_camera_update(camera, delta_time);
    return 0;
}

static int lua_camera_process_keyboard(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    int key = (int)luaL_checkinteger(L, 2);
    bool pressed = lua_toboolean(L, 3);
    double delta_time = luaL_checknumber(L, 4);

    if (!camera) {
        lua_pushstring(L, "Invalid camera object");
        lua_error(L);
        return 0;
    }

    poc_camera_process_keyboard(camera, key, pressed, delta_time);
    return 0;
}

static int lua_camera_process_mouse_movement(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    double mouse_x = luaL_checknumber(L, 2);
    double mouse_y = luaL_checknumber(L, 3);
    bool constrain_pitch = lua_toboolean(L, 4);

    if (!camera) {
        lua_pushstring(L, "Invalid camera object");
        lua_error(L);
        return 0;
    }

    poc_camera_process_mouse_movement(camera, mouse_x, mouse_y, constrain_pitch);
    return 0;
}

static int lua_camera_process_mouse_scroll(lua_State *L) {
    poc_camera *camera = check_camera(L, 1);
    double scroll_y = luaL_checknumber(L, 2);

    if (!camera) {
        lua_pushstring(L, "Invalid camera object");
        lua_error(L);
        return 0;
    }

    poc_camera_process_mouse_scroll(camera, scroll_y);
    return 0;
}

// Vec3 userdata for position properties
typedef struct {
    vec3 *vec;
    poc_camera *camera;
    char property[32];
} vec3_userdata;

static void push_vec3_userdata(lua_State *L, vec3 *v, poc_camera *camera, const char *property) {
    vec3_userdata *userdata = (vec3_userdata *)lua_newuserdata(L, sizeof(vec3_userdata));
    userdata->vec = v;
    userdata->camera = camera;
    strncpy(userdata->property, property, sizeof(userdata->property) - 1);
    userdata->property[sizeof(userdata->property) - 1] = '\0';

    luaL_getmetatable(L, VEC3_METATABLE);
    lua_setmetatable(L, -2);
}

static int lua_vec3_index(lua_State *L) {
    vec3_userdata *userdata = (vec3_userdata *)luaL_checkudata(L, 1, VEC3_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (!userdata || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, (*userdata->vec)[0]);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, (*userdata->vec)[1]);
        return 1;
    } else if (strcmp(key, "z") == 0) {
        lua_pushnumber(L, (*userdata->vec)[2]);
        return 1;
    }

    return 0;
}

static int lua_vec3_newindex(lua_State *L) {
    vec3_userdata *userdata = (vec3_userdata *)luaL_checkudata(L, 1, VEC3_METATABLE);
    const char *key = luaL_checkstring(L, 2);
    double value = luaL_checknumber(L, 3);

    if (!userdata || !key) return 0;

    if (strcmp(key, "x") == 0) {
        (*userdata->vec)[0] = (float)value;
        if (userdata->camera) userdata->camera->matrices_dirty = true;
    } else if (strcmp(key, "y") == 0) {
        (*userdata->vec)[1] = (float)value;
        if (userdata->camera) userdata->camera->matrices_dirty = true;
    } else if (strcmp(key, "z") == 0) {
        (*userdata->vec)[2] = (float)value;
        if (userdata->camera) userdata->camera->matrices_dirty = true;
    } else {
        luaL_error(L, "Invalid vec3 property '%s'", key);
    }

    return 0;
}

static int lua_vec3_gc(lua_State *L) {
    (void)L; // Unused parameter
    // vec3_userdata doesn't own the vec3, so no cleanup needed
    return 0;
}





// Utility functions

// lua_push_mat4 function removed as it's not currently used