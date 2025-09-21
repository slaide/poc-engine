/**
 * @file scripting.h
 * @brief Lua/Teal scripting system for POC Engine
 *
 * This module provides Lua scripting capabilities with Teal type checking.
 * It manages the Lua state, loads and executes scripts, and provides
 * error handling and debugging features.
 */

#pragma once

#include "poc_engine.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to the scripting system
 *
 * Manages the Lua state and provides script execution capabilities.
 * Each context can have its own scripting instance for isolation.
 */
typedef struct poc_scripting poc_scripting;

/**
 * @brief Script execution result codes
 */
typedef enum {
    POC_SCRIPT_SUCCESS = 0,              /**< Script executed successfully */
    POC_SCRIPT_ERROR_INIT_FAILED,        /**< Failed to initialize Lua state */
    POC_SCRIPT_ERROR_FILE_NOT_FOUND,     /**< Script file not found */
    POC_SCRIPT_ERROR_SYNTAX_ERROR,       /**< Lua syntax error */
    POC_SCRIPT_ERROR_RUNTIME_ERROR,      /**< Runtime error during execution */
    POC_SCRIPT_ERROR_TEAL_TYPE_ERROR,    /**< Teal type checking failed */
    POC_SCRIPT_ERROR_OUT_OF_MEMORY,      /**< Out of memory */
} poc_script_result;

/**
 * @brief Script execution configuration
 */
typedef struct {
    bool enable_teal_checking;    /**< Enable Teal type checking before execution */
    bool enable_debug_info;       /**< Include debug information in error messages */
    const char *script_path;      /**< Base path for script files */
} poc_script_config;

/**
 * @brief Initialize the scripting system
 *
 * Creates a new Lua state and initializes the standard libraries.
 * Registers POC Engine API functions for use in scripts.
 *
 * @param config Configuration for the scripting system. Must not be NULL.
 * @return Pointer to the new scripting system on success, or NULL on failure
 *
 * @note Must call poc_scripting_shutdown() to clean up resources.
 */
poc_scripting *poc_scripting_init(const poc_script_config *config);

/**
 * @brief Shut down the scripting system
 *
 * Destroys the Lua state and frees all associated resources.
 *
 * @param scripting The scripting system to shut down. Can be NULL (no-op).
 */
void poc_scripting_shutdown(poc_scripting *scripting);

/**
 * @brief Load and execute a Lua script file
 *
 * Loads the specified script file and executes it in the Lua state.
 * If Teal checking is enabled, validates the script before execution.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @param filename Path to the script file to load. Must not be NULL.
 * @return POC_SCRIPT_SUCCESS on success, or an error code on failure
 *
 * @note File path is relative to the script_path set in configuration.
 */
poc_script_result poc_scripting_load_file(poc_scripting *scripting, const char *filename);

/**
 * @brief Execute a Lua script from a string
 *
 * Executes the provided Lua code string in the current Lua state.
 * If Teal checking is enabled, validates the script before execution.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @param script_code The Lua code to execute. Must not be NULL.
 * @param script_name Optional name for error reporting. Can be NULL.
 * @return POC_SCRIPT_SUCCESS on success, or an error code on failure
 */
poc_script_result poc_scripting_execute_string(poc_scripting *scripting,
                                               const char *script_code,
                                               const char *script_name);

/**
 * @brief Load and execute a Teal script file
 *
 * Loads a .tl file, performs type checking, compiles to Lua, and executes.
 * This is the preferred way to run typed scripts.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @param filename Path to the .tl file to load. Must not be NULL.
 * @return POC_SCRIPT_SUCCESS on success, or an error code on failure
 *
 * @note Requires the Teal compiler to be available in deps/teal/tl.lua
 */
poc_script_result poc_scripting_load_teal_file(poc_scripting *scripting, const char *filename);

/**
 * @brief Call a Lua function by name
 *
 * Calls a global Lua function with the specified arguments and returns the result.
 * Arguments are passed as a variable argument list.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @param function_name Name of the Lua function to call. Must not be NULL.
 * @param arg_format Format string describing arguments (e.g., "dd" for two doubles).
 * @param ... Variable arguments matching the format string.
 * @return POC_SCRIPT_SUCCESS on success, or an error code on failure
 *
 * @note Format string uses: 'd' for double, 'i' for int, 's' for string, 'b' for boolean
 */
poc_script_result poc_scripting_call_function(poc_scripting *scripting,
                                              const char *function_name,
                                              const char *arg_format, ...);

/**
 * @brief Set a global variable in the Lua state
 *
 * Sets a global variable that can be accessed by Lua scripts.
 * Useful for passing data from C to Lua.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @param name Name of the global variable. Must not be NULL.
 * @param value_format Format character ('d', 'i', 's', 'b') for the value type.
 * @param value The value to set (must match the format).
 * @return POC_SCRIPT_SUCCESS on success, or an error code on failure
 */
poc_script_result poc_scripting_set_global(poc_scripting *scripting,
                                           const char *name,
                                           char value_format,
                                           void *value);

/**
 * @brief Get a global variable from the Lua state
 *
 * Retrieves a global variable value from Lua and stores it in the provided buffer.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @param name Name of the global variable. Must not be NULL.
 * @param value_format Format character ('d', 'i', 's', 'b') for the expected type.
 * @param value Buffer to store the retrieved value. Must not be NULL.
 * @return POC_SCRIPT_SUCCESS on success, or an error code on failure
 */
poc_script_result poc_scripting_get_global(poc_scripting *scripting,
                                           const char *name,
                                           char value_format,
                                           void *value);

/**
 * @brief Get the last error message from the scripting system
 *
 * Returns a human-readable description of the last error that occurred.
 * Useful for debugging and error reporting.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @return Pointer to a static string describing the last error.
 *         Never returns NULL.
 */
const char *poc_scripting_get_last_error(poc_scripting *scripting);

/**
 * @brief Convert a script result code to a human-readable string
 *
 * Converts a poc_script_result error code into a descriptive string.
 *
 * @param result The result code to convert
 * @return Pointer to a static string describing the result.
 *         Never returns NULL.
 */
const char *poc_script_result_to_string(poc_script_result result);

/**
 * @brief Get the current Lua state for advanced operations
 *
 * Provides access to the underlying lua_State for advanced users
 * who need to call Lua C API functions directly.
 *
 * @param scripting The scripting system. Must not be NULL.
 * @return Pointer to the lua_State, or NULL if not initialized
 *
 * @warning Use with caution. Modifying the Lua state directly can
 *          break the scripting system's internal state.
 */
lua_State *poc_scripting_get_lua_state(poc_scripting *scripting);

// Forward declarations for integration
struct poc_camera;
struct podi_window;

/**
 * @brief Set the active rendering context for camera binding
 *
 * Sets the rendering context that cameras can be bound to.
 * This must be called before using POC.bind_camera() in Lua scripts.
 *
 * @param context Pointer to the rendering context.
 *                Can be NULL to disable camera binding.
 */
void poc_scripting_set_context(struct poc_context *context);

/**
 * @brief Set the active window for application control
 *
 * Sets the window that can be controlled from Lua scripts.
 * This enables Lua to quit the application or control window behavior.
 *
 * @param window Pointer to the window.
 *               Can be NULL to disable window control.
 */
void poc_scripting_set_window(struct podi_window *window);

#ifdef __cplusplus
}
#endif