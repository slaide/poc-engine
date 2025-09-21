#include "scripting.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Forward declaration for Lua bindings
extern void poc_scripting_register_bindings(lua_State *L);
extern void poc_math_register_bindings(lua_State *L);

struct poc_scripting {
    lua_State *L;
    poc_script_config config;
    char last_error[1024];
};

poc_scripting *poc_scripting_init(const poc_script_config *config) {
    if (!config) {
        printf("poc_scripting_init: config cannot be NULL\n");
        return NULL;
    }

    poc_scripting *scripting = malloc(sizeof(poc_scripting));
    if (!scripting) {
        printf("poc_scripting_init: failed to allocate scripting system\n");
        return NULL;
    }

    // Initialize the Lua state
    scripting->L = luaL_newstate();
    if (!scripting->L) {
        printf("poc_scripting_init: failed to create Lua state\n");
        free(scripting);
        return NULL;
    }

    // Open standard Lua libraries
    luaL_openlibs(scripting->L);

    // Copy configuration
    scripting->config = *config;
    if (config->script_path) {
        // We need to store a copy of the string since config might be temporary
        // For now, we'll just point to it - in a real implementation we'd strdup
        // This is a limitation that should be addressed
    }

    // Clear error message
    scripting->last_error[0] = '\0';

    // Register POC Engine API bindings
    poc_scripting_register_bindings(scripting->L);
    poc_math_register_bindings(scripting->L);

    // If Teal checking is enabled, load the Teal compiler
    if (config->enable_teal_checking) {
        int result = luaL_dofile(scripting->L, "deps/teal/tl.lua");
        if (result != LUA_OK) {
            snprintf(scripting->last_error, sizeof(scripting->last_error),
                    "Failed to load Teal compiler: %s", lua_tostring(scripting->L, -1));
            lua_pop(scripting->L, 1);
            // Continue without Teal - just log the warning
            printf("Warning: %s\n", scripting->last_error);
        } else {
            printf("✓ Teal compiler loaded successfully\n");
        }
    }

    printf("POC Engine scripting system initialized\n");
    return scripting;
}

void poc_scripting_shutdown(poc_scripting *scripting) {
    if (!scripting) {
        return;
    }

    if (scripting->L) {
        lua_close(scripting->L);
    }

    free(scripting);
    printf("POC Engine scripting system shut down\n");
}

poc_script_result poc_scripting_load_file(poc_scripting *scripting, const char *filename) {
    if (!scripting || !filename) {
        return POC_SCRIPT_ERROR_INIT_FAILED;
    }

    // Construct full path if script_path is set
    char full_path[1024];
    if (scripting->config.script_path) {
        snprintf(full_path, sizeof(full_path), "%s/%s", scripting->config.script_path, filename);
    } else {
        strncpy(full_path, filename, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    // Check if file exists
    FILE *file = fopen(full_path, "r");
    if (!file) {
        snprintf(scripting->last_error, sizeof(scripting->last_error),
                "Script file not found: %.500s (%.100s)", full_path, strerror(errno));
        return POC_SCRIPT_ERROR_FILE_NOT_FOUND;
    }
    fclose(file);

    // Load and execute the file
    int result = luaL_dofile(scripting->L, full_path);
    if (result != LUA_OK) {
        snprintf(scripting->last_error, sizeof(scripting->last_error),
                "Lua error: %s", lua_tostring(scripting->L, -1));
        lua_pop(scripting->L, 1);

        if (result == LUA_ERRSYNTAX) {
            return POC_SCRIPT_ERROR_SYNTAX_ERROR;
        } else if (result == LUA_ERRMEM) {
            return POC_SCRIPT_ERROR_OUT_OF_MEMORY;
        } else {
            return POC_SCRIPT_ERROR_RUNTIME_ERROR;
        }
    }

    return POC_SCRIPT_SUCCESS;
}

poc_script_result poc_scripting_execute_string(poc_scripting *scripting,
                                               const char *script_code,
                                               const char *script_name) {
    if (!scripting || !script_code) {
        return POC_SCRIPT_ERROR_INIT_FAILED;
    }

    const char *name = script_name ? script_name : "string";
    int result = luaL_loadbuffer(scripting->L, script_code, strlen(script_code), name);

    if (result != LUA_OK) {
        snprintf(scripting->last_error, sizeof(scripting->last_error),
                "Lua syntax error: %s", lua_tostring(scripting->L, -1));
        lua_pop(scripting->L, 1);
        return POC_SCRIPT_ERROR_SYNTAX_ERROR;
    }

    result = lua_pcall(scripting->L, 0, 0, 0);
    if (result != LUA_OK) {
        snprintf(scripting->last_error, sizeof(scripting->last_error),
                "Lua runtime error: %s", lua_tostring(scripting->L, -1));
        lua_pop(scripting->L, 1);

        if (result == LUA_ERRMEM) {
            return POC_SCRIPT_ERROR_OUT_OF_MEMORY;
        } else {
            return POC_SCRIPT_ERROR_RUNTIME_ERROR;
        }
    }

    return POC_SCRIPT_SUCCESS;
}

poc_script_result poc_scripting_load_teal_file(poc_scripting *scripting, const char *filename) {
    if (!scripting || !filename) {
        return POC_SCRIPT_ERROR_INIT_FAILED;
    }

    // For now, we'll implement a simple version that just calls the regular load_file
    // A complete implementation would:
    // 1. Load the .tl file
    // 2. Call tl.check() to type-check the code
    // 3. Call tl.gen() to generate Lua code
    // 4. Execute the generated Lua code

    printf("Warning: Teal file loading not fully implemented yet, falling back to Lua\n");
    return poc_scripting_load_file(scripting, filename);
}

poc_script_result poc_scripting_call_function(poc_scripting *scripting,
                                              const char *function_name,
                                              const char *arg_format, ...) {
    if (!scripting || !function_name) {
        return POC_SCRIPT_ERROR_INIT_FAILED;
    }

    // Get the function from the global table
    lua_getglobal(scripting->L, function_name);
    if (!lua_isfunction(scripting->L, -1)) {
        snprintf(scripting->last_error, sizeof(scripting->last_error),
                "Function '%s' not found or not callable", function_name);
        lua_pop(scripting->L, 1);
        return POC_SCRIPT_ERROR_RUNTIME_ERROR;
    }

    // Push arguments based on format string
    int arg_count = 0;
    if (arg_format) {
        va_list args;
        va_start(args, arg_format);

        for (const char *fmt = arg_format; *fmt; fmt++) {
            switch (*fmt) {
                case 'd': {
                    double value = va_arg(args, double);
                    lua_pushnumber(scripting->L, value);
                    arg_count++;
                    break;
                }
                case 'i': {
                    int value = va_arg(args, int);
                    lua_pushinteger(scripting->L, value);
                    arg_count++;
                    break;
                }
                case 's': {
                    const char *value = va_arg(args, const char*);
                    lua_pushstring(scripting->L, value ? value : "");
                    arg_count++;
                    break;
                }
                case 'b': {
                    int value = va_arg(args, int);
                    lua_pushboolean(scripting->L, value);
                    arg_count++;
                    break;
                }
                default:
                    snprintf(scripting->last_error, sizeof(scripting->last_error),
                            "Unknown format character '%c' in argument format", *fmt);
                    va_end(args);
                    lua_pop(scripting->L, arg_count + 1); // +1 for the function
                    return POC_SCRIPT_ERROR_RUNTIME_ERROR;
            }
        }

        va_end(args);
    }

    // Call the function
    int result = lua_pcall(scripting->L, arg_count, 0, 0);
    if (result != LUA_OK) {
        snprintf(scripting->last_error, sizeof(scripting->last_error),
                "Error calling function '%s': %s", function_name, lua_tostring(scripting->L, -1));
        lua_pop(scripting->L, 1);

        if (result == LUA_ERRMEM) {
            return POC_SCRIPT_ERROR_OUT_OF_MEMORY;
        } else {
            return POC_SCRIPT_ERROR_RUNTIME_ERROR;
        }
    }

    return POC_SCRIPT_SUCCESS;
}

poc_script_result poc_scripting_set_global(poc_scripting *scripting,
                                           const char *name,
                                           char value_format,
                                           void *value) {
    if (!scripting || !name || !value) {
        return POC_SCRIPT_ERROR_INIT_FAILED;
    }

    switch (value_format) {
        case 'd': {
            double *d_value = (double*)value;
            lua_pushnumber(scripting->L, *d_value);
            break;
        }
        case 'i': {
            int *i_value = (int*)value;
            lua_pushinteger(scripting->L, *i_value);
            break;
        }
        case 's': {
            const char **s_value = (const char**)value;
            lua_pushstring(scripting->L, *s_value ? *s_value : "");
            break;
        }
        case 'b': {
            bool *b_value = (bool*)value;
            lua_pushboolean(scripting->L, *b_value);
            break;
        }
        default:
            snprintf(scripting->last_error, sizeof(scripting->last_error),
                    "Unknown format character '%c' for global variable", value_format);
            return POC_SCRIPT_ERROR_RUNTIME_ERROR;
    }

    lua_setglobal(scripting->L, name);
    return POC_SCRIPT_SUCCESS;
}

poc_script_result poc_scripting_get_global(poc_scripting *scripting,
                                           const char *name,
                                           char value_format,
                                           void *value) {
    if (!scripting || !name || !value) {
        return POC_SCRIPT_ERROR_INIT_FAILED;
    }

    lua_getglobal(scripting->L, name);

    switch (value_format) {
        case 'd': {
            if (!lua_isnumber(scripting->L, -1)) {
                snprintf(scripting->last_error, sizeof(scripting->last_error),
                        "Global variable '%s' is not a number", name);
                lua_pop(scripting->L, 1);
                return POC_SCRIPT_ERROR_RUNTIME_ERROR;
            }
            double *d_value = (double*)value;
            *d_value = lua_tonumber(scripting->L, -1);
            break;
        }
        case 'i': {
            if (!lua_isinteger(scripting->L, -1)) {
                snprintf(scripting->last_error, sizeof(scripting->last_error),
                        "Global variable '%s' is not an integer", name);
                lua_pop(scripting->L, 1);
                return POC_SCRIPT_ERROR_RUNTIME_ERROR;
            }
            int *i_value = (int*)value;
            *i_value = (int)lua_tointeger(scripting->L, -1);
            break;
        }
        case 's': {
            if (!lua_isstring(scripting->L, -1)) {
                snprintf(scripting->last_error, sizeof(scripting->last_error),
                        "Global variable '%s' is not a string", name);
                lua_pop(scripting->L, 1);
                return POC_SCRIPT_ERROR_RUNTIME_ERROR;
            }
            const char **s_value = (const char**)value;
            *s_value = lua_tostring(scripting->L, -1);
            // Note: This string becomes invalid after lua_pop, so caller should copy it
            break;
        }
        case 'b': {
            bool *b_value = (bool*)value;
            *b_value = lua_toboolean(scripting->L, -1);
            break;
        }
        default:
            snprintf(scripting->last_error, sizeof(scripting->last_error),
                    "Unknown format character '%c' for global variable", value_format);
            lua_pop(scripting->L, 1);
            return POC_SCRIPT_ERROR_RUNTIME_ERROR;
    }

    lua_pop(scripting->L, 1);
    return POC_SCRIPT_SUCCESS;
}

const char *poc_scripting_get_last_error(poc_scripting *scripting) {
    if (!scripting) {
        return "Scripting system not initialized";
    }
    return scripting->last_error[0] ? scripting->last_error : "No error";
}

const char *poc_script_result_to_string(poc_script_result result) {
    switch (result) {
        case POC_SCRIPT_SUCCESS: return "Success";
        case POC_SCRIPT_ERROR_INIT_FAILED: return "Initialization failed";
        case POC_SCRIPT_ERROR_FILE_NOT_FOUND: return "Script file not found";
        case POC_SCRIPT_ERROR_SYNTAX_ERROR: return "Syntax error";
        case POC_SCRIPT_ERROR_RUNTIME_ERROR: return "Runtime error";
        case POC_SCRIPT_ERROR_TEAL_TYPE_ERROR: return "Teal type checking failed";
        case POC_SCRIPT_ERROR_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

lua_State *poc_scripting_get_lua_state(poc_scripting *scripting) {
    return scripting ? scripting->L : NULL;
}