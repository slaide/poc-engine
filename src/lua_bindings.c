#include "scripting.h"
#include "camera.h"
#include "poc_engine.h"
#include "vulkan_renderer.h"
#include "math_bindings.h"
#include "scene.h"
#include "scene_object.h"
#include "mesh.h"
#include <podi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Metatable names for userdata
#define CAMERA_METATABLE "POCEngine.Camera"
#define VEC3_METATABLE "POCEngine.Vec3"
#define SCENE_METATABLE "POCEngine.Scene"
#define SCENE_OBJECT_METATABLE "POCEngine.SceneObject"
#define MESH_METATABLE "POCEngine.Mesh"

// Forward declarations for binding functions
static int lua_poc_get_time(lua_State *L);
static int lua_poc_sleep(lua_State *L);
static int lua_poc_create_camera(lua_State *L);
static int lua_poc_bind_camera(lua_State *L);
static int lua_poc_camera_set_fov(lua_State *L);
static int lua_poc_camera_set_vertical_fov(lua_State *L);
static int lua_poc_camera_set_horizontal_fov(lua_State *L);
static int lua_poc_camera_get_vertical_fov(lua_State *L);
static int lua_poc_camera_get_horizontal_fov(lua_State *L);
static int lua_poc_quit_application(lua_State *L);
static int lua_poc_set_cursor_mode(lua_State *L);
static int lua_poc_get_cursor_position(lua_State *L);

// Scene system bindings
static int lua_poc_create_scene(lua_State *L);
static int lua_poc_bind_scene(lua_State *L);
static int lua_poc_create_scene_object(lua_State *L);
static int lua_poc_load_mesh(lua_State *L);
static int lua_poc_pick_object(lua_State *L);
static int lua_poc_scene_add_object(lua_State *L);
static int lua_poc_scene_object_set_mesh(lua_State *L);
static int lua_poc_scene_object_set_position(lua_State *L);
static int lua_poc_scene_save(lua_State *L);
static int lua_poc_scene_load(lua_State *L);
static int lua_poc_scene_clone(lua_State *L);
static int lua_poc_scene_copy_from(lua_State *L);
static int lua_poc_set_play_mode(lua_State *L);
static int lua_poc_is_play_mode(lua_State *L);

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
struct poc_context *g_active_context = NULL;

// Global window for application control
static podi_window *g_active_window = NULL;

// Global scene and camera for picking
static poc_scene *g_active_scene = NULL;
static poc_camera *g_active_camera = NULL;

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

    // Create Scene metatable
    luaL_newmetatable(L, SCENE_METATABLE);
    lua_pop(L, 1);

    // Create SceneObject metatable
    luaL_newmetatable(L, SCENE_OBJECT_METATABLE);
    lua_pop(L, 1);

    // Create Mesh metatable
    luaL_newmetatable(L, MESH_METATABLE);
    lua_pop(L, 1);

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

    lua_pushcfunction(L, lua_poc_camera_set_fov);
    lua_setfield(L, -2, "camera_set_fov");

    lua_pushcfunction(L, lua_poc_camera_set_vertical_fov);
    lua_setfield(L, -2, "camera_set_vertical_fov");

    lua_pushcfunction(L, lua_poc_camera_set_horizontal_fov);
    lua_setfield(L, -2, "camera_set_horizontal_fov");

    lua_pushcfunction(L, lua_poc_camera_get_vertical_fov);
    lua_setfield(L, -2, "camera_get_vertical_fov");

    lua_pushcfunction(L, lua_poc_camera_get_horizontal_fov);
    lua_setfield(L, -2, "camera_get_horizontal_fov");

    // Application control
    lua_pushcfunction(L, lua_poc_quit_application);
    lua_setfield(L, -2, "quit_application");

    // Scene system
    lua_pushcfunction(L, lua_poc_create_scene);
    lua_setfield(L, -2, "create_scene");

    lua_pushcfunction(L, lua_poc_bind_scene);
    lua_setfield(L, -2, "bind_scene");

    lua_pushcfunction(L, lua_poc_create_scene_object);
    lua_setfield(L, -2, "create_scene_object");

    lua_pushcfunction(L, lua_poc_load_mesh);
    lua_setfield(L, -2, "load_mesh");

    lua_pushcfunction(L, lua_poc_pick_object);
    lua_setfield(L, -2, "pick_object");

    lua_pushcfunction(L, lua_poc_scene_add_object);
    lua_setfield(L, -2, "scene_add_object");

    lua_pushcfunction(L, lua_poc_scene_object_set_mesh);
    lua_setfield(L, -2, "scene_object_set_mesh");

    lua_pushcfunction(L, lua_poc_scene_object_set_position);
    lua_setfield(L, -2, "scene_object_set_position");

    lua_pushcfunction(L, lua_poc_scene_save);
    lua_setfield(L, -2, "scene_save");

    lua_pushcfunction(L, lua_poc_scene_load);
    lua_setfield(L, -2, "scene_load");

    lua_pushcfunction(L, lua_poc_scene_clone);
    lua_setfield(L, -2, "scene_clone");

    lua_pushcfunction(L, lua_poc_scene_copy_from);
    lua_setfield(L, -2, "scene_copy_from");

    // Cursor control functions
    lua_pushcfunction(L, lua_poc_set_cursor_mode);
    lua_setfield(L, -2, "set_cursor_mode");

    lua_pushcfunction(L, lua_poc_get_cursor_position);
    lua_setfield(L, -2, "get_cursor_position");

    // Play/Edit mode toggle
    lua_pushcfunction(L, lua_poc_set_play_mode);
    lua_setfield(L, -2, "set_play_mode");

    lua_pushcfunction(L, lua_poc_is_play_mode);
    lua_setfield(L, -2, "is_play_mode");

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
    static const struct {
        const char *name;
        podi_key value;
    } key_constants[] = {
        {"UNKNOWN", PODI_KEY_UNKNOWN},
        {"A", PODI_KEY_A}, {"B", PODI_KEY_B}, {"C", PODI_KEY_C}, {"D", PODI_KEY_D},
        {"E", PODI_KEY_E}, {"F", PODI_KEY_F}, {"G", PODI_KEY_G}, {"H", PODI_KEY_H},
        {"I", PODI_KEY_I}, {"J", PODI_KEY_J}, {"K", PODI_KEY_K}, {"L", PODI_KEY_L},
        {"M", PODI_KEY_M}, {"N", PODI_KEY_N}, {"O", PODI_KEY_O}, {"P", PODI_KEY_P},
        {"Q", PODI_KEY_Q}, {"R", PODI_KEY_R}, {"S", PODI_KEY_S}, {"T", PODI_KEY_T},
        {"U", PODI_KEY_U}, {"V", PODI_KEY_V}, {"W", PODI_KEY_W}, {"X", PODI_KEY_X},
        {"Y", PODI_KEY_Y}, {"Z", PODI_KEY_Z},
        {"D0", PODI_KEY_0}, {"D1", PODI_KEY_1}, {"D2", PODI_KEY_2}, {"D3", PODI_KEY_3},
        {"D4", PODI_KEY_4}, {"D5", PODI_KEY_5}, {"D6", PODI_KEY_6}, {"D7", PODI_KEY_7},
        {"D8", PODI_KEY_8}, {"D9", PODI_KEY_9},
        {"SPACE", PODI_KEY_SPACE}, {"ENTER", PODI_KEY_ENTER}, {"ESCAPE", PODI_KEY_ESCAPE},
        {"BACKSPACE", PODI_KEY_BACKSPACE}, {"TAB", PODI_KEY_TAB},
        {"SHIFT", PODI_KEY_SHIFT}, {"CTRL", PODI_KEY_CTRL}, {"ALT", PODI_KEY_ALT},
        {"UP", PODI_KEY_UP}, {"DOWN", PODI_KEY_DOWN},
        {"LEFT", PODI_KEY_LEFT}, {"RIGHT", PODI_KEY_RIGHT},
    };
    for (size_t i = 0; i < sizeof(key_constants) / sizeof(key_constants[0]); ++i) {
        lua_pushinteger(L, key_constants[i].value);
        lua_setfield(L, -2, key_constants[i].name);
    }
    lua_setglobal(L, "KEY");

    // Register mouse button constants
    lua_newtable(L);
    static const struct {
        const char *name;
        podi_mouse_button value;
    } mouse_constants[] = {
        {"LEFT", PODI_MOUSE_BUTTON_LEFT},
        {"RIGHT", PODI_MOUSE_BUTTON_RIGHT},
        {"MIDDLE", PODI_MOUSE_BUTTON_MIDDLE},
        {"X1", PODI_MOUSE_BUTTON_X1},
        {"X2", PODI_MOUSE_BUTTON_X2},
    };
    for (size_t i = 0; i < sizeof(mouse_constants) / sizeof(mouse_constants[0]); ++i) {
        lua_pushinteger(L, mouse_constants[i].value);
        lua_setfield(L, -2, mouse_constants[i].name);
    }
    lua_setglobal(L, "MOUSE_BUTTON");

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

    // Store camera globally for picking
    g_active_camera = camera;

    return 0;
}

static void ensure_active_camera(lua_State *L) {
    if (!g_active_camera) {
        lua_pushstring(L, "No active camera bound");
        lua_error(L);
    }
}

static int lua_poc_camera_set_fov(lua_State *L) {
    ensure_active_camera(L);
    float fov = (float)luaL_checknumber(L, 1);
    poc_camera_set_vertical_fov(g_active_camera, fov);
    return 0;
}

static int lua_poc_camera_set_vertical_fov(lua_State *L) {
    ensure_active_camera(L);
    float fov = (float)luaL_checknumber(L, 1);
    poc_camera_set_vertical_fov(g_active_camera, fov);
    return 0;
}

static int lua_poc_camera_set_horizontal_fov(lua_State *L) {
    ensure_active_camera(L);
    float fov = (float)luaL_checknumber(L, 1);
    poc_camera_set_horizontal_fov(g_active_camera, fov);
    return 0;
}

static int lua_poc_camera_get_vertical_fov(lua_State *L) {
    ensure_active_camera(L);
    lua_pushnumber(L, poc_camera_get_vertical_fov(g_active_camera));
    return 1;
}

static int lua_poc_camera_get_horizontal_fov(lua_State *L) {
    ensure_active_camera(L);
    lua_pushnumber(L, poc_camera_get_horizontal_fov(g_active_camera));
    return 1;
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

// Get the currently active scene (set by Lua script)
poc_scene *poc_scripting_get_active_scene(void) {
    if (g_active_context) {
        poc_scene *context_scene = poc_context_get_active_scene(g_active_context);
        if (context_scene) {
            g_active_scene = context_scene;
        }
    }
    return g_active_scene;
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
    } else if (strcmp(key, "fov") == 0 || strcmp(key, "vertical_fov") == 0) {
        lua_pushnumber(L, poc_camera_get_vertical_fov(camera));
        return 1;
    } else if (strcmp(key, "horizontal_fov") == 0) {
        lua_pushnumber(L, poc_camera_get_horizontal_fov(camera));
        return 1;
    } else if (strcmp(key, "fov_mode") == 0) {
        const char *mode = camera->fov_mode == POC_CAMERA_FOV_HORIZONTAL ? "horizontal" : "vertical";
        lua_pushstring(L, mode);
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
        poc_camera_update_vectors(camera);
        camera->matrices_dirty = true;
    } else if (strcmp(key, "pitch") == 0) {
        camera->pitch = (float)luaL_checknumber(L, 3);
        poc_camera_update_vectors(camera);
        camera->matrices_dirty = true;
    } else if (strcmp(key, "roll") == 0) {
        camera->roll = (float)luaL_checknumber(L, 3);
        poc_camera_update_vectors(camera);
        camera->matrices_dirty = true;
    } else if (strcmp(key, "fov") == 0 || strcmp(key, "vertical_fov") == 0) {
        float value = (float)luaL_checknumber(L, 3);
        poc_camera_set_vertical_fov(camera, value);
    } else if (strcmp(key, "horizontal_fov") == 0) {
        float value = (float)luaL_checknumber(L, 3);
        poc_camera_set_horizontal_fov(camera, value);
    } else if (strcmp(key, "fov_mode") == 0) {
        luaL_error(L, "camera.fov_mode is read-only");
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

// Scene system function implementations

static int lua_poc_create_scene(lua_State *L) {
    poc_scene *scene = poc_scene_create();
    if (!scene) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to create scene");
        return 2;
    }

    poc_scene **userdata = (poc_scene **)lua_newuserdata(L, sizeof(poc_scene *));
    *userdata = scene;
    luaL_setmetatable(L, SCENE_METATABLE);
    return 1;
}

static int lua_poc_bind_scene(lua_State *L) {
    poc_scene **scene_ptr = (poc_scene **)luaL_checkudata(L, 1, SCENE_METATABLE);
    if (!scene_ptr || !*scene_ptr) {
        lua_pushstring(L, "Invalid scene object");
        lua_error(L);
        return 0;
    }

    g_active_scene = *scene_ptr;
    printf("✓ Scene bound for picking\n");

    if (g_active_context) {
        poc_context_set_scene(g_active_context, g_active_scene);
        printf("[playmode] Context scene updated from Lua bind\n");
    }
    return 0;
}

static int lua_poc_create_scene_object(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    uint32_t id = (uint32_t)luaL_checkinteger(L, 2);

    poc_scene_object *obj = poc_scene_object_create(name, id);
    if (!obj) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to create scene object");
        return 2;
    }

    poc_scene_object **userdata = (poc_scene_object **)lua_newuserdata(L, sizeof(poc_scene_object *));
    *userdata = obj;
    luaL_setmetatable(L, SCENE_OBJECT_METATABLE);
    return 1;
}

static int lua_poc_load_mesh(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);

    poc_mesh *mesh = poc_mesh_load(filename);
    if (!mesh) {
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to load mesh from '%s'", filename);
        return 2;
    }

    poc_mesh **userdata = (poc_mesh **)lua_newuserdata(L, sizeof(poc_mesh *));
    *userdata = mesh;
    luaL_setmetatable(L, MESH_METATABLE);
    return 1;
}

static int lua_poc_pick_object(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);

    if (!g_active_scene || !g_active_camera) {
        lua_pushnil(L);
        lua_pushstring(L, "No active scene or camera for picking");
        return 2;
    }

    // Convert screen coordinates to normalized coordinates [0,1]
    float norm_x = x / (float)width;
    float norm_y = y / (float)height;

    printf("🎯 PICKING DEBUG: Screen (%.1f, %.1f) -> Normalized (%.3f, %.3f)\n", x, y, norm_x, norm_y);

    // Generate ray from camera through screen point
    poc_ray ray;
    if (!poc_camera_screen_to_ray(g_active_camera, norm_x, norm_y, &ray)) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to generate picking ray");
        return 2;
    }

    printf("🎯 RAY: Origin (%.2f, %.2f, %.2f) Direction (%.3f, %.3f, %.3f)\n",
           ray.origin[0], ray.origin[1], ray.origin[2],
           ray.direction[0], ray.direction[1], ray.direction[2]);

    // Camera debug
    printf("🎯 CAMERA: Position (%.2f, %.2f, %.2f) Yaw %.1f° Pitch %.1f°\n",
           g_active_camera->position[0], g_active_camera->position[1], g_active_camera->position[2],
           g_active_camera->yaw, g_active_camera->pitch);

    // Scene objects debug
    printf("🎯 SCENE: %d objects to test\n", g_active_scene->object_count);
    for (uint32_t i = 0; i < g_active_scene->object_count; i++) {
        poc_scene_object *obj = g_active_scene->objects[i];
        if (obj) {
            printf("🎯 OBJECT[%d]: %s ID=%d Position(%.2f, %.2f, %.2f) Renderable=%s\n",
                   i, obj->name, obj->id,
                   obj->position[0], obj->position[1], obj->position[2],
                   poc_scene_object_is_renderable(obj) ? "YES" : "NO");
            if (poc_scene_object_is_renderable(obj)) {
                printf("🎯   AABB: Min(%.2f, %.2f, %.2f) Max(%.2f, %.2f, %.2f)\n",
                       obj->world_aabb_min[0], obj->world_aabb_min[1], obj->world_aabb_min[2],
                       obj->world_aabb_max[0], obj->world_aabb_max[1], obj->world_aabb_max[2]);
            }
        }
    }

    // Perform picking
    poc_hit_result hit;
    if (poc_scene_pick_object(g_active_scene, &ray, &hit) && hit.hit) {
        // Create hit result table
        lua_newtable(L);

        lua_pushboolean(L, true);
        lua_setfield(L, -2, "hit");

        lua_pushinteger(L, hit.object->id);
        lua_setfield(L, -2, "object_id");

        lua_pushstring(L, hit.object->name);
        lua_setfield(L, -2, "object_name");

        lua_pushnumber(L, hit.distance);
        lua_setfield(L, -2, "distance");

        // Hit point
        lua_newtable(L);
        lua_pushnumber(L, hit.point[0]);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, hit.point[1]);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, hit.point[2]);
        lua_setfield(L, -2, "z");
        lua_setfield(L, -2, "point");

        return 1;
    } else {
        // No hit
        lua_newtable(L);
        lua_pushboolean(L, false);
        lua_setfield(L, -2, "hit");
        return 1;
    }
}

static int lua_poc_scene_add_object(lua_State *L) {
    poc_scene **scene_ptr = (poc_scene **)luaL_checkudata(L, 1, SCENE_METATABLE);
    poc_scene_object **obj_ptr = (poc_scene_object **)luaL_checkudata(L, 2, SCENE_OBJECT_METATABLE);

    if (!scene_ptr || !*scene_ptr || !obj_ptr || !*obj_ptr) {
        lua_pushboolean(L, false);
        return 1;
    }

    bool success = poc_scene_add_object(*scene_ptr, *obj_ptr);
    lua_pushboolean(L, success);
    return 1;
}

static int lua_poc_scene_object_set_mesh(lua_State *L) {
    poc_scene_object **obj_ptr = (poc_scene_object **)luaL_checkudata(L, 1, SCENE_OBJECT_METATABLE);
    poc_mesh **mesh_ptr = (poc_mesh **)luaL_checkudata(L, 2, MESH_METATABLE);

    if (!obj_ptr || !*obj_ptr || !mesh_ptr || !*mesh_ptr) {
        return 0;
    }

    poc_scene_object_set_mesh(*obj_ptr, *mesh_ptr);
    return 0;
}

static int lua_poc_scene_object_set_position(lua_State *L) {
    poc_scene_object **obj_ptr = (poc_scene_object **)luaL_checkudata(L, 1, SCENE_OBJECT_METATABLE);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);

    if (!obj_ptr || !*obj_ptr) {
        return 0;
    }

    vec3 position = {x, y, z};
    poc_scene_object_set_position(*obj_ptr, position);
    return 0;
}

static int lua_poc_scene_save(lua_State *L) {
    poc_scene **scene_ptr = (poc_scene **)luaL_checkudata(L, 1, SCENE_METATABLE);
    const char *path = luaL_checkstring(L, 2);

    if (!scene_ptr || !*scene_ptr) {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid scene object");
        return 2;
    }

    if (!poc_scene_save_to_file(*scene_ptr, path)) {
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to save scene to '%s'", path);
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_poc_scene_load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    poc_scene *scene = poc_scene_load_from_file(path);
    if (!scene) {
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to load scene from '%s'", path);
        return 2;
    }

    poc_scene **userdata = (poc_scene **)lua_newuserdata(L, sizeof(poc_scene *));
    *userdata = scene;
    luaL_setmetatable(L, SCENE_METATABLE);
    return 1;
}

static int lua_poc_scene_clone(lua_State *L) {
    poc_scene **scene_ptr = (poc_scene **)luaL_checkudata(L, 1, SCENE_METATABLE);
    if (!scene_ptr || !*scene_ptr) {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid scene object");
        return 2;
    }

    poc_scene *clone = poc_scene_clone(*scene_ptr);
    if (!clone) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to clone scene");
        return 2;
    }

    poc_scene **userdata = (poc_scene **)lua_newuserdata(L, sizeof(poc_scene *));
    *userdata = clone;
    luaL_setmetatable(L, SCENE_METATABLE);
    return 1;
}

static int lua_poc_scene_copy_from(lua_State *L) {
    poc_scene **dest_ptr = (poc_scene **)luaL_checkudata(L, 1, SCENE_METATABLE);
    poc_scene **src_ptr = (poc_scene **)luaL_checkudata(L, 2, SCENE_METATABLE);

    if (!dest_ptr || !*dest_ptr || !src_ptr || !*src_ptr) {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid scene arguments");
        return 2;
    }

    if (!poc_scene_copy_from(*dest_ptr, *src_ptr)) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to copy scene contents");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_poc_set_play_mode(lua_State *L) {
    bool enabled = lua_toboolean(L, 1);

    if (!g_active_context) {
        lua_pushstring(L, "No active context set - cannot set play mode");
        lua_error(L);
        return 0;
    }

    printf("[playmode] lua set_play_mode -> %s\n", enabled ? "true" : "false");
    poc_context_set_play_mode(g_active_context, enabled);
    g_active_scene = poc_context_get_active_scene(g_active_context);
    return 0;
}

static int lua_poc_is_play_mode(lua_State *L) {
    bool is_play_mode = false;

    if (g_active_context) {
        is_play_mode = poc_context_is_play_mode(g_active_context);
    }

    lua_pushboolean(L, is_play_mode);
    return 1;
}

static int lua_poc_set_cursor_mode(lua_State *L) {
    bool locked = lua_toboolean(L, 1);
    bool visible = lua_toboolean(L, 2);

    if (!g_active_window) {
        lua_pushstring(L, "No active window set - cannot set cursor mode");
        lua_error(L);
        return 0;
    }

    printf("[playmode] lua set_cursor_mode: locked=%s visible=%s\n",
           locked ? "true" : "false", visible ? "true" : "false");
    podi_window_set_cursor_mode(g_active_window, locked, visible);
    return 0;
}

static int lua_poc_get_cursor_position(lua_State *L) {
    if (!g_active_window) {
        lua_pushstring(L, "No active window set - cannot get cursor position");
        lua_error(L);
        return 0;
    }

    double x, y;
    podi_window_get_cursor_position(g_active_window, &x, &y);

    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// Utility functions

// lua_push_mat4 function removed as it's not currently used
