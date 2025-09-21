#ifdef POC_PLATFORM_LINUX

#include "vulkan_renderer.h"
#include "poc_engine.h"
#include "obj_loader.h"
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <cglm/cglm.h>
#include "../deps/podi/src/internal.h"

// Forward declare X11 types to avoid including X11 headers
typedef struct _Display Display;
typedef unsigned long Window;
typedef unsigned long VisualID;

// Forward declare Wayland types
struct wl_display;
struct wl_surface;

// Define podi handle types (only available in podi with PODI_PLATFORM_LINUX)
typedef struct {
    void *display;
    unsigned long window;
} podi_x11_handles;

typedef struct {
    void *display;
    void *surface;
} podi_wayland_handles;

// Declare podi handle getter functions
bool podi_window_get_x11_handles(podi_window *window, podi_x11_handles *handles);
bool podi_window_get_wayland_handles(podi_window *window, podi_wayland_handles *handles);

// Forward declarations for depth resource management
static void cleanup_depth_resources(poc_context *ctx);
static poc_result create_depth_resources(poc_context *ctx);

// Title bar height constant (logical pixels) for client-side decorations
#define PODI_TITLE_BAR_HEIGHT 40

// Include Vulkan platform-specific headers after X11 types are defined
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>

// Uniform buffer object structure matching the shader
typedef struct {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 ambient_color;
    float _pad1;
    vec3 diffuse_color;
    float _pad2;
    vec3 specular_color;
    float shininess;
    vec3 light_pos;
    float _pad3;
    vec3 view_pos;
    float _pad4;
} UniformBufferObject;

// Renderable object structure
struct poc_renderable {
    // Geometry data
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    uint32_t vertex_count;
    uint32_t index_count;

    // Per-object uniform resources
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;
    void *uniform_buffer_mapped;
    VkDescriptorSet descriptor_set;

    // Material properties
    poc_material material;
    bool has_material;

    // Transform
    mat4 model_matrix;

    // Identification
    char name[256];

    // Context reference (for resource cleanup)
    poc_context *ctx;
};

#define VK_CHECK(result) \
    do { \
        VkResult vk_result = (result); \
        if (vk_result != VK_SUCCESS) { \
            printf("Vulkan error: %d at %s:%d\n", vk_result, __FILE__, __LINE__); \
            return POC_RESULT_ERROR_INIT_FAILED; \
        } \
    } while (0)

typedef struct {
    bool x11_support;
    bool wayland_support;
} surface_support;

typedef struct {
    uint32_t graphics_family;
    uint32_t present_family;
    bool graphics_family_found;
    bool present_family_found;
} queue_family_indices;

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_family_index;
    uint32_t present_family_index;
    bool validation_enabled;
    surface_support surface_caps;
} vulkan_state;

#define MAX_FRAMES_IN_FLIGHT 2

struct poc_context {
    vulkan_state *vk;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkColorSpaceKHR swapchain_colorspace;
    VkExtent2D swapchain_extent;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    uint32_t swapchain_image_count;
    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;
    VkSemaphore *image_available_semaphores;  // One per swapchain image
    VkSemaphore *render_finished_semaphores;  // One per swapchain image
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];  // Keep per-frame fences
    uint32_t current_frame;
    uint32_t current_image_index;
    uint32_t current_acquire_semaphore_index;  // Track which semaphore was used for acquisition
    float clear_color[4];
    podi_window *window;

    // Resize handling
    bool needs_swapchain_recreation;
    uint32_t last_known_width;
    uint32_t last_known_height;

    // Rendering pipeline
    VkRenderPass render_pass;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer *framebuffers;
    uint32_t framebuffer_count;  // Track framebuffer count independently from swapchain
    VkShaderModule vert_shader_module;
    VkShaderModule frag_shader_module;

    // Shared descriptor resources
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet *descriptor_sets;  // DEPRECATED - kept for fallback compatibility

    // Camera/transformation data
    float camera_rotation_y;

    // Model rendering support (DEPRECATED - use renderables instead)
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    uint32_t current_vertex_count;
    uint32_t current_index_count;

    // New renderable system
    poc_renderable **renderables;
    uint32_t renderable_count;
    uint32_t renderable_capacity;

    // Depth buffer
    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
};

static vulkan_state g_vk_state = {0};

static const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

static const char *get_format_string(VkFormat format) {
    switch (format) {
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        default: return "UNKNOWN_FORMAT";
    }
}

static const char *get_present_mode_string(VkPresentModeKHR present_mode) {
    switch (present_mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "VK_PRESENT_MODE_IMMEDIATE_KHR";
        case VK_PRESENT_MODE_MAILBOX_KHR: return "VK_PRESENT_MODE_MAILBOX_KHR";
        case VK_PRESENT_MODE_FIFO_KHR: return "VK_PRESENT_MODE_FIFO_KHR";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
        default: return "UNKNOWN_PRESENT_MODE";
    }
}

static const char *get_colorspace_string(VkColorSpaceKHR colorspace) {
    switch (colorspace) {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
        case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT";
        case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT";
        case VK_COLOR_SPACE_BT709_LINEAR_EXT: return "VK_COLOR_SPACE_BT709_LINEAR_EXT";
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: return "VK_COLOR_SPACE_BT709_NONLINEAR_EXT";
        case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "VK_COLOR_SPACE_BT2020_LINEAR_EXT";
        case VK_COLOR_SPACE_HDR10_ST2084_EXT: return "VK_COLOR_SPACE_HDR10_ST2084_EXT";
        case VK_COLOR_SPACE_DOLBYVISION_EXT: return "VK_COLOR_SPACE_DOLBYVISION_EXT";
        case VK_COLOR_SPACE_HDR10_HLG_EXT: return "VK_COLOR_SPACE_HDR10_HLG_EXT";
        case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: return "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT";
        case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: return "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT";
        case VK_COLOR_SPACE_PASS_THROUGH_EXT: return "VK_COLOR_SPACE_PASS_THROUGH_EXT";
        case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT";
        default: return "UNKNOWN_COLORSPACE";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {

    (void)message_type;
    (void)user_data;

    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        printf("Vulkan validation layer: %s\n", callback_data->pMessage);
    }

    return VK_FALSE;
}

static bool check_validation_layer_support(void) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties *available_layers = malloc(layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    bool found = false;
    for (uint32_t i = 0; i < sizeof(validation_layers) / sizeof(validation_layers[0]); i++) {
        for (uint32_t j = 0; j < layer_count; j++) {
            if (strcmp(validation_layers[i], available_layers[j].layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    free(available_layers);
    return found;
}

static void list_available_layers(void) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    if (layer_count == 0) {
        printf("No Vulkan layers available\n");
        return;
    }

    VkLayerProperties *layers = malloc(layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layer_count, layers);

    printf("=== Available Vulkan Layers (%u) ===\n", layer_count);
    for (uint32_t i = 0; i < layer_count; i++) {
        printf("  [%u] %s\n", i, layers[i].layerName);
        printf("      Spec Version: %u.%u.%u\n",
               VK_VERSION_MAJOR(layers[i].specVersion),
               VK_VERSION_MINOR(layers[i].specVersion),
               VK_VERSION_PATCH(layers[i].specVersion));
        printf("      Implementation: %u\n", layers[i].implementationVersion);
        printf("      Description: %s\n", layers[i].description);
        printf("\n");
    }

    free(layers);
}

static void list_instance_extensions(void) {
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

    if (extension_count == 0) {
        printf("No Vulkan instance extensions available\n");
        return;
    }

    VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions);

    printf("=== Available Instance Extensions (%u) ===\n", extension_count);
    for (uint32_t i = 0; i < extension_count; i++) {
        printf("  [%u] %s (version %u)\n", i, extensions[i].extensionName, extensions[i].specVersion);
    }
    printf("\n");

    free(extensions);
}

static poc_result create_instance(const poc_config *config) {
    VkApplicationInfo app_info = (VkApplicationInfo){
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = config->app_name ? config->app_name : "POC Engine App",
        .applicationVersion = config->app_version,
        .pEngineName = "POC Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo create_info = (VkInstanceCreateInfo){
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };

    // Build extension list dynamically based on availability
    const char *required_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME};

    const char **extensions = malloc(10 * sizeof(char*)); // Max 10 extensions
    uint32_t extension_count = 0;

    // Add required extensions
    for (uint32_t i = 0; i < sizeof(required_extensions) / sizeof(required_extensions[0]); i++) {
        extensions[extension_count++] = required_extensions[i];
    }

    // Add debug extension if validation is enabled
    if (config->enable_validation) {
        extensions[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    // Add surface extensions based on support
    if (g_vk_state.surface_caps.x11_support) {
        extensions[extension_count++] = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
        printf("Adding X11 surface extension to instance\n");
    }

    if (g_vk_state.surface_caps.wayland_support) {
        extensions[extension_count++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
        printf("Adding Wayland surface extension to instance\n");
    }

    printf("Creating Vulkan instance with %u extensions:\n", extension_count);
    for (uint32_t i = 0; i < extension_count; i++) {
        printf("  %s\n", extensions[i]);
    }

    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;

    if (config->enable_validation && check_validation_layer_support()) {
        create_info.enabledLayerCount = sizeof(validation_layers) / sizeof(validation_layers[0]);
        create_info.ppEnabledLayerNames = validation_layers;
        g_vk_state.validation_enabled = true;
        printf("✓ Vulkan validation layers ENABLED\n");
        for (uint32_t i = 0; i < create_info.enabledLayerCount; i++) {
            printf("  - %s\n", validation_layers[i]);
        }
    } else {
        create_info.enabledLayerCount = 0;
        g_vk_state.validation_enabled = false;
        if (config->enable_validation) {
            printf("⚠ Validation requested but layers not available\n");
        } else {
            printf("○ Vulkan validation layers DISABLED\n");
        }
    }

    VK_CHECK(vkCreateInstance(&create_info, NULL, &g_vk_state.instance));

    free(extensions);
    return POC_RESULT_SUCCESS;
}

static poc_result setup_debug_messenger(void) {
    if (!g_vk_state.validation_enabled) {
        printf("○ Debug messenger not needed (validation disabled)\n");
        return POC_RESULT_SUCCESS;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = (VkDebugUtilsMessengerCreateInfoEXT){
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback
    };

    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(g_vk_state.instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != NULL) {
        VK_CHECK(func(g_vk_state.instance, &create_info, NULL, &g_vk_state.debug_messenger));
        printf("✓ Debug messenger created - validation messages will be displayed\n");
    } else {
        printf("⚠ Debug messenger creation function not found\n");
    }

    return POC_RESULT_SUCCESS;
}

static const char *get_device_type_string(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
        default: return "Unknown";
    }
}


static void enumerate_device_extensions(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    if (extension_count > 0) {
        VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions);

        printf("    Device Extensions (%u):\n", extension_count);
        for (uint32_t i = 0; i < extension_count; i++) {
            printf("      %s (version %u)\n", extensions[i].extensionName, extensions[i].specVersion);
        }

        free(extensions);
    } else {
        printf("    No device extensions available\n");
    }
}

static VkSurfaceKHR create_surface(podi_window *window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    podi_backend_type backend = podi_get_backend();

    if (backend == PODI_BACKEND_X11) {
        podi_x11_handles x11_handles;
        if (!podi_window_get_x11_handles(window, &x11_handles)) {
            printf("FATAL ERROR: Failed to get X11 handles from window\n");
            exit(1);
        }

        VkXlibSurfaceCreateInfoKHR surface_info = {
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy = (Display*)x11_handles.display,
            .window = (Window)x11_handles.window
        };

        VkResult result = vkCreateXlibSurfaceKHR(g_vk_state.instance, &surface_info, NULL, &surface);
        if (result != VK_SUCCESS) {
            printf("FATAL ERROR: Failed to create X11 surface (error: %d)\n", result);
            exit(1);
        }

        printf("✓ X11 surface created successfully\n");
    } else if (backend == PODI_BACKEND_WAYLAND) {
        podi_wayland_handles wayland_handles;
        if (!podi_window_get_wayland_handles(window, &wayland_handles)) {
            printf("FATAL ERROR: Failed to get Wayland handles from window\n");
            exit(1);
        }

        VkWaylandSurfaceCreateInfoKHR surface_info = {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = (struct wl_display*)wayland_handles.display,
            .surface = (struct wl_surface*)wayland_handles.surface
        };

        VkResult result = vkCreateWaylandSurfaceKHR(g_vk_state.instance, &surface_info, NULL, &surface);
        if (result != VK_SUCCESS) {
            printf("FATAL ERROR: Failed to create Wayland surface (error: %d)\n", result);
            exit(1);
        }

        printf("✓ Wayland surface created successfully\n");
    } else {
        printf("FATAL ERROR: Unsupported backend type: %d\n", backend);
        exit(1);
    }

    return surface;
}

static queue_family_indices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    queue_family_indices indices = {0};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties *queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_family_found = true;
        }

        // For dummy surface, assume the graphics queue family also supports present
        // In real implementation, this would check actual surface support
        if (surface == (VkSurfaceKHR)0x1) {
            // Dummy surface - assume graphics queue can present
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.present_family = i;
                indices.present_family_found = true;
            }
        } else {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
                indices.present_family = i;
                indices.present_family_found = true;
            }
        }

        if (indices.graphics_family_found && indices.present_family_found) {
            break;
        }
    }

    free(queue_families);
    return indices;
}

static uint32_t rate_device_suitability(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    // Check if device has required queue families
    queue_family_indices indices = find_queue_families(device, surface);
    if (!indices.graphics_family_found || !indices.present_family_found) {
        return 0; // Device is not suitable
    }

    // Check for required swapchain extension support
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    bool has_swapchain = false;
    if (extension_count > 0) {
        VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions);

        for (uint32_t i = 0; i < extension_count; i++) {
            if (strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                has_swapchain = true;
                break;
            }
        }

        free(extensions);
    }

    if (!has_swapchain) {
        return 0; // Device is not suitable without swapchain support
    }

    // Device type scores (higher is better)
    uint32_t score = 0;
    switch (device_properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 100;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 50;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 10;
            break;
        default:
            score += 1;
            break;
    }

    return score;
}

static poc_result select_physical_device(VkSurfaceKHR surface) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vk_state.instance, &device_count, NULL);

    if (device_count == 0) {
        printf("No Vulkan physical devices found\n");
        return POC_RESULT_ERROR_DEVICE_NOT_FOUND;
    }

    VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(g_vk_state.instance, &device_count, devices);

    printf("=== Physical Device Selection ===\n");

    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    uint32_t best_score = 0;
    queue_family_indices best_indices = {0};

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        uint32_t score = rate_device_suitability(devices[i], surface);
        queue_family_indices indices = find_queue_families(devices[i], surface);

        printf("  [%u] %s (%s)\n", i, properties.deviceName, get_device_type_string(properties.deviceType));
        printf("      Score: %u%s\n", score, score == 0 ? " (UNSUITABLE)" : "");
        printf("      Graphics queue: %s (family %u)\n",
               indices.graphics_family_found ? "YES" : "NO",
               indices.graphics_family_found ? indices.graphics_family : 0);
        printf("      Present queue: %s (family %u)\n",
               indices.present_family_found ? "YES" : "NO",
               indices.present_family_found ? indices.present_family : 0);
        printf("      Swapchain support: %s\n", score > 0 ? "YES" : "NO");

        if (score > best_score) {
            best_device = devices[i];
            best_score = score;
            best_indices = indices;
        }

        printf("\n");
    }

    if (best_device == VK_NULL_HANDLE) {
        printf("No suitable physical device found\n");
        free(devices);
        return POC_RESULT_ERROR_DEVICE_NOT_FOUND;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(best_device, &properties);

    printf("✓ Selected device: %s (score: %u)\n", properties.deviceName, best_score);
    printf("  Graphics queue family: %u\n", best_indices.graphics_family);
    printf("  Present queue family: %u\n", best_indices.present_family);

    g_vk_state.physical_device = best_device;
    g_vk_state.graphics_family_index = best_indices.graphics_family;
    g_vk_state.present_family_index = best_indices.present_family;

    free(devices);
    return POC_RESULT_SUCCESS;
}

static poc_result enumerate_physical_devices(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vk_state.instance, &device_count, NULL);

    if (device_count == 0) {
        printf("No Vulkan physical devices found\n");
        return POC_RESULT_ERROR_DEVICE_NOT_FOUND;
    }

    VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(g_vk_state.instance, &device_count, devices);

    printf("=== Physical Devices (%u) ===\n", device_count);

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceMemoryProperties memory_properties;

        vkGetPhysicalDeviceProperties(devices[i], &properties);
        vkGetPhysicalDeviceFeatures(devices[i], &features);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memory_properties);

        printf("  [%u] %s\n", i, properties.deviceName);
        printf("    Type: %s\n", get_device_type_string(properties.deviceType));
        printf("    Vendor ID: 0x%x\n", properties.vendorID);
        printf("    Device ID: 0x%x\n", properties.deviceID);
        printf("    Driver Version: %u\n", properties.driverVersion);
        printf("    API Version: %u.%u.%u\n",
               VK_VERSION_MAJOR(properties.apiVersion),
               VK_VERSION_MINOR(properties.apiVersion),
               VK_VERSION_PATCH(properties.apiVersion));

        // Queue families
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, NULL);

        VkQueueFamilyProperties *queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, queue_families);

        printf("    Queue Families (%u):\n", queue_family_count);
        for (uint32_t j = 0; j < queue_family_count; j++) {
            printf("      [%u] Queue Count: %u, Flags:", j, queue_families[j].queueCount);
            if (queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) printf(" GRAPHICS");
            if (queue_families[j].queueFlags & VK_QUEUE_COMPUTE_BIT) printf(" COMPUTE");
            if (queue_families[j].queueFlags & VK_QUEUE_TRANSFER_BIT) printf(" TRANSFER");
            if (queue_families[j].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) printf(" SPARSE_BINDING");
            printf("\n");
        }
        free(queue_families);

        // Memory properties
        printf("    Memory Heaps (%u):\n", memory_properties.memoryHeapCount);
        for (uint32_t j = 0; j < memory_properties.memoryHeapCount; j++) {
            printf("      [%u] Size: %lu MB, Flags:", j, memory_properties.memoryHeaps[j].size / 1024 / 1024);
            if (memory_properties.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) printf(" DEVICE_LOCAL");
            printf("\n");
        }

        // Device extensions
        enumerate_device_extensions(devices[i]);

        printf("\n");
    }

    free(devices);

    // Intentionally not selecting any device for now
    printf("NOTE: No physical device selected intentionally - enumeration complete\n");
    return POC_RESULT_ERROR_DEVICE_NOT_FOUND; // This will cause graceful exit
}

static bool check_extension_support(const char *extension_name) {
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

    VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions);

    bool found = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    free(extensions);
    return found;
}

static surface_support check_surface_extensions(void) {
    surface_support support = {0};

    printf("=== Surface Extension Support ===\n");

    support.x11_support = check_extension_support(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    printf("  X11 (VK_KHR_xlib_surface): %s\n", support.x11_support ? "SUPPORTED" : "NOT SUPPORTED");

    support.wayland_support = check_extension_support(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    printf("  Wayland (VK_KHR_wayland_surface): %s\n", support.wayland_support ? "SUPPORTED" : "NOT SUPPORTED");

    printf("\n");
    return support;
}


static podi_backend_type determine_compatible_backend(void) {
    printf("=== Determining Compatible Window Backend ===\n");

    // Check what Podi is currently using
    podi_backend_type current_backend = podi_get_backend();
    const char *current_backend_name = podi_get_backend_name();
    printf("Current Podi backend: %s\n", current_backend_name);

    // Check if current backend is supported by Vulkan
    bool current_supported = false;
    if (current_backend == PODI_BACKEND_X11 && g_vk_state.surface_caps.x11_support) {
        current_supported = true;
        printf("Current X11 backend is supported by Vulkan\n");
    } else if (current_backend == PODI_BACKEND_WAYLAND && g_vk_state.surface_caps.wayland_support) {
        current_supported = true;
        printf("Current Wayland backend is supported by Vulkan\n");
    }

    if (current_supported) {
        printf("Using current backend: %s\n", current_backend_name);
        return current_backend;
    }

    // Try to find a compatible backend
    printf("Current backend not supported by Vulkan, searching for alternatives...\n");

    if (g_vk_state.surface_caps.wayland_support) {
        printf("Setting backend to Wayland (preferred)\n");
        podi_set_backend(PODI_BACKEND_WAYLAND);
        return PODI_BACKEND_WAYLAND;
    } else if (g_vk_state.surface_caps.x11_support) {
        printf("Setting backend to X11 (fallback)\n");
        podi_set_backend(PODI_BACKEND_X11);
        return PODI_BACKEND_X11;
    } else {
        printf("ERROR: No compatible window backend found!\n");
        printf("  X11 support: %s\n", g_vk_state.surface_caps.x11_support ? "YES" : "NO");
        printf("  Wayland support: %s\n", g_vk_state.surface_caps.wayland_support ? "YES" : "NO");
        return PODI_BACKEND_AUTO; // Will cause an error
    }
}


poc_result vulkan_init(const poc_config *config) {
    poc_result result;

    // List all available Vulkan layers
    list_available_layers();

    // List all available instance extensions
    list_instance_extensions();

    // Check surface extension support before creating instance
    g_vk_state.surface_caps = check_surface_extensions();

    // Determine and set compatible window backend before creating instance
    podi_backend_type backend = determine_compatible_backend();
    if (backend == PODI_BACKEND_AUTO) {
        printf("ERROR: Failed to determine compatible window backend\n");
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    result = create_instance(config);
    if (result != POC_RESULT_SUCCESS) return result;

    result = setup_debug_messenger();
    if (result != POC_RESULT_SUCCESS) return result;

    // Enumerate all physical devices with detailed information (for debugging)
    result = enumerate_physical_devices();
    if (result != POC_RESULT_SUCCESS) {
        printf("Device enumeration completed\n");
    }

    printf("=== Vulkan Initialization Summary ===\n");
    printf("Selected window backend: %s\n", podi_get_backend_name());
    printf("Surface extensions available: X11=%s, Wayland=%s\n",
           g_vk_state.surface_caps.x11_support ? "YES" : "NO",
           g_vk_state.surface_caps.wayland_support ? "YES" : "NO");
    printf("NOTE: Physical device will be selected when creating context\n");

    return POC_RESULT_SUCCESS;
}

void vulkan_shutdown(void) {
    if (g_vk_state.device) {
        vkDestroyDevice(g_vk_state.device, NULL);
        g_vk_state.device = VK_NULL_HANDLE;
    }

    if (g_vk_state.debug_messenger && g_vk_state.validation_enabled) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vk_state.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL) {
            func(g_vk_state.instance, g_vk_state.debug_messenger, NULL);
        }
        g_vk_state.debug_messenger = VK_NULL_HANDLE;
    }

    if (g_vk_state.instance) {
        vkDestroyInstance(g_vk_state.instance, NULL);
        g_vk_state.instance = VK_NULL_HANDLE;
    }
}

static poc_result create_logical_device(void) {
    // Check for unique queue families
    bool graphics_and_present_same = (g_vk_state.graphics_family_index == g_vk_state.present_family_index);
    uint32_t unique_queue_families[] = {g_vk_state.graphics_family_index, g_vk_state.present_family_index};
    uint32_t queue_family_count = graphics_and_present_same ? 1 : 2;

    VkDeviceQueueCreateInfo queue_create_infos[2];
    float queue_priority = 1.0f;

    for (uint32_t i = 0; i < queue_family_count; i++) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_queue_families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
    }

    // Required device extensions
    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures device_features = {0};

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_create_infos,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]),
        .ppEnabledExtensionNames = device_extensions
    };

    // Enable validation layers on device level for older Vulkan implementations
    if (g_vk_state.validation_enabled) {
        create_info.enabledLayerCount = sizeof(validation_layers) / sizeof(validation_layers[0]);
        create_info.ppEnabledLayerNames = validation_layers;
    }

    VK_CHECK(vkCreateDevice(g_vk_state.physical_device, &create_info, NULL, &g_vk_state.device));

    // Retrieve queues
    vkGetDeviceQueue(g_vk_state.device, g_vk_state.graphics_family_index, 0, &g_vk_state.graphics_queue);
    vkGetDeviceQueue(g_vk_state.device, g_vk_state.present_family_index, 0, &g_vk_state.present_queue);

    printf("✓ Logical device created\n");
    printf("  Graphics queue family: %u\n", g_vk_state.graphics_family_index);
    printf("  Present queue family: %u\n", g_vk_state.present_family_index);

    return POC_RESULT_SUCCESS;
}

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t format_count;
    VkPresentModeKHR *present_modes;
    uint32_t present_mode_count;
} swapchain_support_details;

static swapchain_support_details query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    swapchain_support_details details = {0};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.format_count, NULL);
    if (details.format_count != 0) {
        details.formats = malloc(details.format_count * sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.format_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_mode_count, NULL);
    if (details.present_mode_count != 0) {
        details.present_modes = malloc(details.present_mode_count * sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_mode_count, details.present_modes);
    }

    return details;
}

static void cleanup_swapchain_support_details(swapchain_support_details *details) {
    if (details->formats) {
        free(details->formats);
        details->formats = NULL;
    }
    if (details->present_modes) {
        free(details->present_modes);
        details->present_modes = NULL;
    }
}

static VkSurfaceFormatKHR choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats, uint32_t format_count) {
    // Prefer SRGB format
    for (uint32_t i = 0; i < format_count; i++) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_formats[i];
        }
    }

    // Fallback to first available format
    return available_formats[0];
}

static VkPresentModeKHR choose_swap_present_mode(const VkPresentModeKHR *available_present_modes, uint32_t present_mode_count) {
    // Prefer mailbox mode (triple buffering)
    for (uint32_t i = 0; i < present_mode_count; i++) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    // Fallback to FIFO (guaranteed to be available)
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR *capabilities, podi_window *window) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    } else {
        int width, height;
        podi_window_get_framebuffer_size(window, &width, &height);

        VkExtent2D actual_extent = {
            (uint32_t)width,
            (uint32_t)height
        };

        actual_extent.width = (actual_extent.width < capabilities->minImageExtent.width) ?
                              capabilities->minImageExtent.width : actual_extent.width;
        actual_extent.width = (actual_extent.width > capabilities->maxImageExtent.width) ?
                              capabilities->maxImageExtent.width : actual_extent.width;

        actual_extent.height = (actual_extent.height < capabilities->minImageExtent.height) ?
                               capabilities->minImageExtent.height : actual_extent.height;
        actual_extent.height = (actual_extent.height > capabilities->maxImageExtent.height) ?
                               capabilities->maxImageExtent.height : actual_extent.height;

        return actual_extent;
    }
}

static poc_result create_swapchain_internal(poc_context *ctx, VkSwapchainKHR old_swapchain) {
    swapchain_support_details swapchain_support = query_swapchain_support(g_vk_state.physical_device, ctx->surface);

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swapchain_support.formats, swapchain_support.format_count);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support.present_modes, swapchain_support.present_mode_count);
    VkExtent2D extent = choose_swap_extent(&swapchain_support.capabilities, ctx->window);

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = ctx->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
    };

    uint32_t queue_family_indices[] = {g_vk_state.graphics_family_index, g_vk_state.present_family_index};

    if (g_vk_state.graphics_family_index != g_vk_state.present_family_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = old_swapchain;

    VK_CHECK(vkCreateSwapchainKHR(g_vk_state.device, &create_info, NULL, &ctx->swapchain));

    // Store swapchain details
    ctx->swapchain_format = surface_format.format;
    ctx->swapchain_colorspace = surface_format.colorSpace;
    ctx->swapchain_extent = extent;

    // Get swapchain images
    vkGetSwapchainImagesKHR(g_vk_state.device, ctx->swapchain, &ctx->swapchain_image_count, NULL);
    ctx->swapchain_images = malloc(ctx->swapchain_image_count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(g_vk_state.device, ctx->swapchain, &ctx->swapchain_image_count, ctx->swapchain_images);

    // Create image views
    ctx->swapchain_image_views = malloc(ctx->swapchain_image_count * sizeof(VkImageView));
    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VkImageViewCreateInfo view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = ctx->swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = ctx->swapchain_format,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };

        VK_CHECK(vkCreateImageView(g_vk_state.device, &view_create_info, NULL, &ctx->swapchain_image_views[i]));
    }

    cleanup_swapchain_support_details(&swapchain_support);

    printf("✓ Swapchain created\n");
    printf("  Format: %s (%d)\n", get_format_string(ctx->swapchain_format), ctx->swapchain_format);
    printf("  Colorspace: %s (%d)\n", get_colorspace_string(ctx->swapchain_colorspace), ctx->swapchain_colorspace);
    printf("  Extent: %ux%u\n", ctx->swapchain_extent.width, ctx->swapchain_extent.height);
    printf("*** SIZE CHECK: Expected 1600x1260, got %ux%u ***\n", ctx->swapchain_extent.width, ctx->swapchain_extent.height);
    printf("  Image count: %u\n", ctx->swapchain_image_count);
    printf("  Present mode: %s (%d)\n", get_present_mode_string(present_mode), present_mode);

    return POC_RESULT_SUCCESS;
}

static poc_result create_swapchain(poc_context *ctx) {
    return create_swapchain_internal(ctx, VK_NULL_HANDLE);
}

static void cleanup_swapchain_images(poc_context *ctx) {
    if (ctx->swapchain_image_views) {
        for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
            if (ctx->swapchain_image_views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(g_vk_state.device, ctx->swapchain_image_views[i], NULL);
            }
        }
        free(ctx->swapchain_image_views);
        ctx->swapchain_image_views = NULL;
    }

    if (ctx->swapchain_images) {
        free(ctx->swapchain_images);
        ctx->swapchain_images = NULL;
    }

    ctx->swapchain_image_count = 0;
}

static void cleanup_pipeline_dependent_resources(poc_context *ctx) {
    if (!ctx || !g_vk_state.device) return;

    // Ensure device is idle before cleanup
    vkDeviceWaitIdle(g_vk_state.device);

    // Destroy framebuffers (dependent on swapchain image views)
    if (ctx->framebuffers) {
        for (uint32_t i = 0; i < ctx->framebuffer_count; i++) {
            if (ctx->framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(g_vk_state.device, ctx->framebuffers[i], NULL);
                ctx->framebuffers[i] = VK_NULL_HANDLE;
            }
        }
        free(ctx->framebuffers);
        ctx->framebuffers = NULL;
        ctx->framebuffer_count = 0;
    }
}

static poc_result create_framebuffers(poc_context *ctx) {
    // Ensure any existing framebuffers are cleaned up first
    if (ctx->framebuffers) {
        cleanup_pipeline_dependent_resources(ctx);
    }

    ctx->framebuffers = malloc(ctx->swapchain_image_count * sizeof(VkFramebuffer));

    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VkImageView attachments[] = {
            ctx->swapchain_image_views[i],
            ctx->depth_image_view
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx->render_pass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = ctx->swapchain_extent.width,
            .height = ctx->swapchain_extent.height,
            .layers = 1
        };

        VK_CHECK(vkCreateFramebuffer(g_vk_state.device, &framebuffer_info, NULL, &ctx->framebuffers[i]));
    }

    ctx->framebuffer_count = ctx->swapchain_image_count;
    printf("✓ Framebuffers created (%u framebuffers)\n", ctx->framebuffer_count);
    return POC_RESULT_SUCCESS;
}

static poc_result recreate_swapchain(poc_context *ctx) {
    printf("Recreating swapchain...\n");

    // Wait for device to be idle
    vkDeviceWaitIdle(g_vk_state.device);

    // Clean up old swapchain resources
    cleanup_swapchain_images(ctx);

    // Clean up pipeline dependent resources (framebuffers)
    cleanup_pipeline_dependent_resources(ctx);

    // Clean up depth resources (they need to match new swapchain size)
    cleanup_depth_resources(ctx);

    VkSwapchainKHR old_swapchain = ctx->swapchain;

    // Create new swapchain
    poc_result result = create_swapchain_internal(ctx, old_swapchain);

    // Destroy old swapchain after creating new one
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_vk_state.device, old_swapchain, NULL);
    }

    if (result != POC_RESULT_SUCCESS) {
        return result;
    }

    // Recreate depth resources with new swapchain size
    result = create_depth_resources(ctx);
    if (result != POC_RESULT_SUCCESS) {
        return result;
    }

    // Recreate framebuffers with new swapchain image views
    result = create_framebuffers(ctx);

    return result;
}

static poc_result create_command_pool(poc_context *ctx) {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = g_vk_state.graphics_family_index
    };

    VK_CHECK(vkCreateCommandPool(g_vk_state.device, &pool_info, NULL, &ctx->command_pool));

    printf("✓ Command pool created\n");
    return POC_RESULT_SUCCESS;
}

static poc_result create_command_buffers(poc_context *ctx) {
    ctx->command_buffers = malloc(ctx->swapchain_image_count * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ctx->swapchain_image_count
    };

    VK_CHECK(vkAllocateCommandBuffers(g_vk_state.device, &alloc_info, ctx->command_buffers));

    printf("✓ Command buffers allocated (%u buffers)\n", ctx->swapchain_image_count);
    return POC_RESULT_SUCCESS;
}

static poc_result create_sync_objects(poc_context *ctx) {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    // Allocate semaphore arrays based on swapchain image count
    ctx->image_available_semaphores = malloc(ctx->swapchain_image_count * sizeof(VkSemaphore));
    ctx->render_finished_semaphores = malloc(ctx->swapchain_image_count * sizeof(VkSemaphore));

    // Create semaphores for each swapchain image
    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VK_CHECK(vkCreateSemaphore(g_vk_state.device, &semaphore_info, NULL, &ctx->image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(g_vk_state.device, &semaphore_info, NULL, &ctx->render_finished_semaphores[i]));
    }

    // Create fences for frames in flight (separate from image count)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(g_vk_state.device, &fence_info, NULL, &ctx->in_flight_fences[i]));
    }

    printf("✓ Synchronization objects created (%u semaphores per type, %d fences)\n",
           ctx->swapchain_image_count, MAX_FRAMES_IN_FLIGHT);
    return POC_RESULT_SUCCESS;
}

static char *read_file(const char *filename, size_t *file_size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(*file_size);
    if (!buffer) {
        printf("Failed to allocate memory for file: %s\n", filename);
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, *file_size, file);
    fclose(file);

    if (read_size != *file_size) {
        printf("Failed to read entire file: %s\n", filename);
        free(buffer);
        return NULL;
    }

    return buffer;
}

static VkShaderModule create_shader_module(const char *filename) {
    size_t code_size;
    char *code = read_file(filename, &code_size);
    if (!code) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = (const uint32_t*)code
    };

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(g_vk_state.device, &create_info, NULL, &shader_module);

    free(code);

    if (result != VK_SUCCESS) {
        printf("Failed to create shader module from %s: %d\n", filename, result);
        return VK_NULL_HANDLE;
    }

    printf("✓ Shader module created from %s\n", filename);
    return shader_module;
}

static VkFormat find_depth_format(void) {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (uint32_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(g_vk_state.physical_device, candidates[i], &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return candidates[i];
        }
    }

    printf("Failed to find supported depth format!\n");
    return VK_FORMAT_UNDEFINED;
}

static poc_result create_render_pass(poc_context *ctx) {
    VkFormat depth_format = find_depth_format();

    VkAttachmentDescription attachments[2] = {
        // Color attachment
        {
            .format = ctx->swapchain_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        // Depth attachment
        {
            .format = depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depth_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(g_vk_state.device, &render_pass_info, NULL, &ctx->render_pass));

    printf("✓ Render pass created with depth attachment\n");
    return POC_RESULT_SUCCESS;
}

static poc_result create_graphics_pipeline(poc_context *ctx) {
    // Load shaders
    ctx->vert_shader_module = create_shader_module("shaders/cube.vert.spv");
    ctx->frag_shader_module = create_shader_module("shaders/cube.frag.spv");

    if (ctx->vert_shader_module == VK_NULL_HANDLE || ctx->frag_shader_module == VK_NULL_HANDLE) {
        printf("Failed to load shader modules\n");
        return POC_RESULT_ERROR_SHADER_COMPILATION_FAILED;
    }

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = ctx->vert_shader_module,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = ctx->frag_shader_module,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    // Vertex input binding description
    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = sizeof(poc_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    // Vertex input attribute descriptions
    VkVertexInputAttributeDescription attribute_descriptions[3] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,  // position (vec3)
            .offset = offsetof(poc_vertex, position)
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,  // normal (vec3)
            .offset = offsetof(poc_vertex, normal)
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,     // texcoord (vec2)
            .offset = offsetof(poc_vertex, texcoord)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = attribute_descriptions
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = NULL,  // Will be set dynamically
        .scissorCount = 1,
        .pScissors = NULL    // Will be set dynamically
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,  // Back-face culling
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,  // Match OBJ convention
        .depthBiasEnable = VK_FALSE
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };

    // Create descriptor set layout for uniform buffer
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_layout_binding
    };

    VK_CHECK(vkCreateDescriptorSetLayout(g_vk_state.device, &layout_info, NULL, &ctx->descriptor_set_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->descriptor_set_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    VK_CHECK(vkCreatePipelineLayout(g_vk_state.device, &pipeline_layout_info, NULL, &ctx->pipeline_layout));

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {}
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = ctx->pipeline_layout,
        .renderPass = ctx->render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VK_CHECK(vkCreateGraphicsPipelines(g_vk_state.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &ctx->graphics_pipeline));

    printf("✓ Graphics pipeline created\n");
    return POC_RESULT_SUCCESS;
}

static uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(g_vk_state.physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    printf("Failed to find suitable memory type!\n");
    return UINT32_MAX;
}

static poc_result create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkBuffer *buffer, VkDeviceMemory *buffer_memory) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(g_vk_state.device, &buffer_info, NULL, buffer));

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(g_vk_state.device, *buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties)
    };

    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &alloc_info, NULL, buffer_memory));
    VK_CHECK(vkBindBufferMemory(g_vk_state.device, *buffer, *buffer_memory, 0));

    return POC_RESULT_SUCCESS;
}

// DEPRECATED: create_uniform_buffers - uniform buffers are now created per-renderable

static poc_result create_descriptor_pool(poc_context *ctx) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = MAX_FRAMES_IN_FLIGHT
    };

    VK_CHECK(vkCreateDescriptorPool(g_vk_state.device, &pool_info, NULL, &ctx->descriptor_pool));

    printf("✓ Descriptor pool created\n");
    return POC_RESULT_SUCCESS;
}

// DEPRECATED: create_descriptor_sets function removed - descriptor sets are now created per-renderable

static poc_result copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size, poc_context *ctx) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = ctx->command_pool,
        .commandBufferCount = 1
    };

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(g_vk_state.device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer
    };

    vkQueueSubmit(g_vk_state.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk_state.graphics_queue);

    vkFreeCommandBuffers(g_vk_state.device, ctx->command_pool, 1, &command_buffer);

    return POC_RESULT_SUCCESS;
}

static poc_result create_vertex_buffer(poc_context *ctx, poc_vertex *vertices, uint32_t vertex_count) {
    VkDeviceSize buffer_size = sizeof(poc_vertex) * vertex_count;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    poc_result result = create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &staging_buffer, &staging_buffer_memory);
    if (result != POC_RESULT_SUCCESS) {
        printf("Failed to create staging buffer for vertex data\n");
        return result;
    }

    void *data;
    vkMapMemory(g_vk_state.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices, buffer_size);
    vkUnmapMemory(g_vk_state.device, staging_buffer_memory);

    result = create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &ctx->vertex_buffer, &ctx->vertex_buffer_memory);
    if (result != POC_RESULT_SUCCESS) {
        printf("Failed to create vertex buffer\n");
        vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
        vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);
        return result;
    }

    copy_buffer(staging_buffer, ctx->vertex_buffer, buffer_size, ctx);

    vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
    vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);

    ctx->current_vertex_count = vertex_count;
    return POC_RESULT_SUCCESS;
}

static poc_result create_index_buffer(poc_context *ctx, uint32_t *indices, uint32_t index_count) {
    VkDeviceSize buffer_size = sizeof(uint32_t) * index_count;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    poc_result result = create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &staging_buffer, &staging_buffer_memory);
    if (result != POC_RESULT_SUCCESS) {
        printf("Failed to create staging buffer for index data\n");
        return result;
    }

    void *data;
    vkMapMemory(g_vk_state.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, indices, buffer_size);
    vkUnmapMemory(g_vk_state.device, staging_buffer_memory);

    result = create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &ctx->index_buffer, &ctx->index_buffer_memory);
    if (result != POC_RESULT_SUCCESS) {
        printf("Failed to create index buffer\n");
        vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
        vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);
        return result;
    }

    copy_buffer(staging_buffer, ctx->index_buffer, buffer_size, ctx);

    vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
    vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);

    ctx->current_index_count = index_count;
    return POC_RESULT_SUCCESS;
}

static void cleanup_depth_resources(poc_context *ctx) {
    if (ctx->depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_vk_state.device, ctx->depth_image_view, NULL);
        ctx->depth_image_view = VK_NULL_HANDLE;
    }
    if (ctx->depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(g_vk_state.device, ctx->depth_image, NULL);
        ctx->depth_image = VK_NULL_HANDLE;
    }
    if (ctx->depth_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, ctx->depth_image_memory, NULL);
        ctx->depth_image_memory = VK_NULL_HANDLE;
    }
}

static poc_result create_depth_resources(poc_context *ctx) {
    VkFormat depth_format = find_depth_format();
    if (depth_format == VK_FORMAT_UNDEFINED) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Create depth image
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = ctx->swapchain_extent.width,
        .extent.height = ctx->swapchain_extent.height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = depth_format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateImage(g_vk_state.device, &image_info, NULL, &ctx->depth_image));

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(g_vk_state.device, ctx->depth_image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &alloc_info, NULL, &ctx->depth_image_memory));
    vkBindImageMemory(g_vk_state.device, ctx->depth_image, ctx->depth_image_memory, 0);

    // Create depth image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };

    VK_CHECK(vkCreateImageView(g_vk_state.device, &view_info, NULL, &ctx->depth_image_view));

    printf("✓ Depth buffer created (%ux%u)\n", ctx->swapchain_extent.width, ctx->swapchain_extent.height);
    return POC_RESULT_SUCCESS;
}


poc_context *vulkan_context_create(podi_window *window) {
    if (!window) {
        printf("vulkan_context_create: window cannot be NULL\n");
        return NULL;
    }

    if (g_vk_state.instance == VK_NULL_HANDLE) {
        printf("vulkan_context_create: Vulkan not initialized\n");
        return NULL;
    }

    printf("=== Creating Vulkan Context ===\n");

    // Create surface from window for device selection
    VkSurfaceKHR surface = create_surface(window);
    if (surface == VK_NULL_HANDLE) {
        return NULL;
    }

    // Select the best physical device based on surface support
    poc_result result = select_physical_device(surface);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        return NULL;
    }

    // Create logical device and retrieve queues
    result = create_logical_device();
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        return NULL;
    }

    // Allocate context
    poc_context *ctx = malloc(sizeof(poc_context));
    if (!ctx) {
        printf("Failed to allocate context memory\n");
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        return NULL;
    }

    memset(ctx, 0, sizeof(poc_context));
    ctx->vk = &g_vk_state;
    ctx->surface = surface;
    ctx->window = window;

    // Initialize renderable array with initial capacity
    ctx->renderable_capacity = 8;
    ctx->renderables = malloc(sizeof(poc_renderable*) * ctx->renderable_capacity);
    if (!ctx->renderables) {
        printf("Failed to allocate renderables array\n");
        free(ctx);
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        return NULL;
    }
    ctx->renderable_count = 0;

    // Initialize resize tracking using surface size (for swapchain)
    int initial_width, initial_height;
    podi_window_get_framebuffer_size(window, &initial_width, &initial_height);
    ctx->last_known_width = (uint32_t)initial_width;
    ctx->last_known_height = (uint32_t)initial_height;
    ctx->needs_swapchain_recreation = false;

    // Set default clear color to pink
    ctx->clear_color[0] = 1.0f;  // Red
    ctx->clear_color[1] = 0.4f;  // Green
    ctx->clear_color[2] = 0.8f;  // Blue
    ctx->clear_color[3] = 1.0f;  // Alpha

    // Create swapchain
    result = create_swapchain(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Create command pool
    result = create_command_pool(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Create command buffers
    result = create_command_buffers(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Create synchronization objects
    result = create_sync_objects(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Create render pass
    result = create_render_pass(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Create graphics pipeline
    result = create_graphics_pipeline(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // NOTE: Uniform buffers are now created per-renderable, not shared

    // Create descriptor pool
    result = create_descriptor_pool(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // NOTE: Descriptor sets are now created per-renderable, not shared

    // Create depth buffer
    result = create_depth_resources(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Create framebuffers
    result = create_framebuffers(ctx);
    if (result != POC_RESULT_SUCCESS) {
        vkDestroySurfaceKHR(g_vk_state.instance, surface, NULL);
        free(ctx);
        return NULL;
    }

    // Initialize current frame
    ctx->current_frame = 0;

    printf("✓ Vulkan context created successfully\n");
    return ctx;
}

void vulkan_context_destroy(poc_context *ctx) {
    if (!ctx) {
        return;
    }

    printf("=== Destroying Vulkan Context ===\n");

    // Wait for device to be idle before cleanup
    if (g_vk_state.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_vk_state.device);
    }

    // Destroy synchronization objects
    // Destroy semaphores (per swapchain image)
    if (ctx->image_available_semaphores) {
        for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
            if (ctx->image_available_semaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(g_vk_state.device, ctx->image_available_semaphores[i], NULL);
            }
        }
        free(ctx->image_available_semaphores);
    }

    if (ctx->render_finished_semaphores) {
        for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
            if (ctx->render_finished_semaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(g_vk_state.device, ctx->render_finished_semaphores[i], NULL);
            }
        }
        free(ctx->render_finished_semaphores);
    }

    // Destroy fences (per frame in flight)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (ctx->in_flight_fences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(g_vk_state.device, ctx->in_flight_fences[i], NULL);
        }
    }

    // Destroy command pool (this also frees command buffers)
    if (ctx->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vk_state.device, ctx->command_pool, NULL);
    }

    // Free command buffers array
    if (ctx->command_buffers) {
        free(ctx->command_buffers);
    }

    // Destroy framebuffers first (dependent on swapchain image views)
    cleanup_pipeline_dependent_resources(ctx);

    // Destroy swapchain
    cleanup_swapchain_images(ctx);
    if (ctx->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_vk_state.device, ctx->swapchain, NULL);
    }

    // NOTE: Uniform buffers are now destroyed per-renderable, not shared

    if (ctx->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_vk_state.device, ctx->descriptor_pool, NULL);
    }

    // NOTE: Descriptor sets are now freed per-renderable, not shared

    if (ctx->descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_vk_state.device, ctx->descriptor_set_layout, NULL);
    }

    // Destroy all renderables
    if (ctx->renderables) {
        for (uint32_t i = 0; i < ctx->renderable_count; i++) {
            poc_renderable *renderable = ctx->renderables[i];
            if (renderable) {
                // Destroy vertex buffer
                if (renderable->vertex_buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(g_vk_state.device, renderable->vertex_buffer, NULL);
                }
                if (renderable->vertex_buffer_memory != VK_NULL_HANDLE) {
                    vkFreeMemory(g_vk_state.device, renderable->vertex_buffer_memory, NULL);
                }
                // Destroy index buffer
                if (renderable->index_buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(g_vk_state.device, renderable->index_buffer, NULL);
                }
                if (renderable->index_buffer_memory != VK_NULL_HANDLE) {
                    vkFreeMemory(g_vk_state.device, renderable->index_buffer_memory, NULL);
                }
                // Destroy per-renderable uniform buffer
                if (renderable->uniform_buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(g_vk_state.device, renderable->uniform_buffer, NULL);
                }
                if (renderable->uniform_buffer_memory != VK_NULL_HANDLE) {
                    vkFreeMemory(g_vk_state.device, renderable->uniform_buffer_memory, NULL);
                }
                free(renderable);
            }
        }
        free(ctx->renderables);
    }

    // Destroy vertex and index buffers (DEPRECATED)
    if (ctx->vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, ctx->vertex_buffer, NULL);
    }
    if (ctx->vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, ctx->vertex_buffer_memory, NULL);
    }
    if (ctx->index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, ctx->index_buffer, NULL);
    }
    if (ctx->index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, ctx->index_buffer_memory, NULL);
    }

    // Destroy depth resources
    cleanup_depth_resources(ctx);

    // Destroy rendering pipeline resources (dependent on render pass)
    if (ctx->graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_vk_state.device, ctx->graphics_pipeline, NULL);
    }

    if (ctx->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_vk_state.device, ctx->pipeline_layout, NULL);
    }

    if (ctx->render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vk_state.device, ctx->render_pass, NULL);
    }

    if (ctx->vert_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_vk_state.device, ctx->vert_shader_module, NULL);
    }

    if (ctx->frag_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(g_vk_state.device, ctx->frag_shader_module, NULL);
    }

    // Destroy surface
    if (ctx->surface != VK_NULL_HANDLE && ctx->surface != (VkSurfaceKHR)0x1 && g_vk_state.instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_vk_state.instance, ctx->surface, NULL);
    }

    free(ctx);
    printf("✓ Vulkan context destroyed\n");
}

static void update_renderable_uniform_buffer(poc_renderable *renderable) {
    if (!renderable || !renderable->uniform_buffer_mapped) {
        return;
    }

    static float time = 0.0f;
    time += 0.016f; // Approximate 60 FPS

    UniformBufferObject ubo = {0};

    // Model matrix from renderable
    memcpy(ubo.model, renderable->model_matrix, sizeof(mat4));

    // View matrix - camera looking at the cubes from further back
    vec3 eye = {0.0f, 2.0f, 6.0f};  // Moved camera back to z=6 and centered on x
    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    glm_lookat(eye, center, up, ubo.view);

    // Projection matrix
    poc_context *ctx = renderable->ctx;
    float aspect_ratio = (float)ctx->swapchain_extent.width / (float)ctx->swapchain_extent.height;
    glm_perspective(glm_rad(45.0f), aspect_ratio, 0.1f, 10.0f, ubo.proj);

    // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    // Since we're using Vulkan, we need to flip the Y coordinate of the projection matrix
    ubo.proj[1][1] *= -1;

    // Material properties
    if (renderable->has_material) {
        const poc_material *material = &renderable->material;
        ubo.ambient_color[0] = material->ambient[0];
        ubo.ambient_color[1] = material->ambient[1];
        ubo.ambient_color[2] = material->ambient[2];
        ubo.diffuse_color[0] = material->diffuse[0];
        ubo.diffuse_color[1] = material->diffuse[1];
        ubo.diffuse_color[2] = material->diffuse[2];
        ubo.specular_color[0] = material->specular[0];
        ubo.specular_color[1] = material->specular[1];
        ubo.specular_color[2] = material->specular[2];
        ubo.shininess = material->shininess;
    } else {
        // Default material
        ubo.ambient_color[0] = 0.2f;
        ubo.ambient_color[1] = 0.2f;
        ubo.ambient_color[2] = 0.2f;
        ubo.diffuse_color[0] = 0.8f;
        ubo.diffuse_color[1] = 0.6f;
        ubo.diffuse_color[2] = 0.4f;
        ubo.specular_color[0] = 1.0f;
        ubo.specular_color[1] = 1.0f;
        ubo.specular_color[2] = 1.0f;
        ubo.shininess = 32.0f;
    }

    // Lighting
    ubo.light_pos[0] = 2.0f;
    ubo.light_pos[1] = 4.0f;
    ubo.light_pos[2] = 2.0f;
    ubo.view_pos[0] = eye[0];
    ubo.view_pos[1] = eye[1];
    ubo.view_pos[2] = eye[2];

    // Copy data to this renderable's uniform buffer
    memcpy(renderable->uniform_buffer_mapped, &ubo, sizeof(ubo));
}

// DEPRECATED: update_uniform_buffer function removed - uniform buffers are now updated per-renderable

#ifdef POC_PLATFORM_LINUX
// Helper function to check if window needs client-side decorations (Linux Wayland only)
static bool needs_client_decorations(podi_window *window) {
    (void)window;  // Unused for now
    // On Linux, check if we're running on Wayland without server decorations
    if (podi_get_backend() == PODI_BACKEND_WAYLAND) {
        // For now, always assume we need client decorations on Wayland
        // since GNOME doesn't support server-side decorations
        return true;
    }
    return false;
}
#endif

static void render_title_bar_if_needed(poc_context *ctx, uint32_t image_index) {
#ifdef POC_PLATFORM_LINUX
    // Only render title bar on Linux Wayland without server decorations
    if (!needs_client_decorations(ctx->window)) {
        return;
    }

    // Get the scale factor to properly size the title bar
    float scale_factor = podi_window_get_scale_factor(ctx->window);
    uint32_t scaled_title_bar_height = (uint32_t)(PODI_TITLE_BAR_HEIGHT * scale_factor);

    // The entire framebuffer was cleared to black in begin_frame
    // Now clear just the content area to the original pink color
    VkRect2D content_scissor = {
        .offset = {0, (int32_t)scaled_title_bar_height},
        .extent = {ctx->swapchain_extent.width, ctx->swapchain_extent.height - scaled_title_bar_height}
    };
    vkCmdSetScissor(ctx->command_buffers[image_index], 0, 1, &content_scissor);

    // Clear the content area to the original background color (pink)
    VkClearAttachment clear_attachment = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .colorAttachment = 0,
        .clearValue.color = {{ctx->clear_color[0], ctx->clear_color[1], ctx->clear_color[2], ctx->clear_color[3]}}
    };

    VkClearRect clear_rect = {
        .rect.offset = {0, (int32_t)scaled_title_bar_height},
        .rect.extent = {ctx->swapchain_extent.width, ctx->swapchain_extent.height - scaled_title_bar_height},
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    vkCmdClearAttachments(ctx->command_buffers[image_index], 1, &clear_attachment, 1, &clear_rect);

    // Keep the scissor set to content area for 3D rendering
#else
    (void)ctx;  // Unused on other platforms
    (void)image_index;
#endif
}

poc_result vulkan_context_begin_frame(poc_context *ctx) {
    if (!ctx) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Check if window size has changed (using surface size for swapchain tracking)
    int current_width, current_height;
    podi_window_get_framebuffer_size(ctx->window, &current_width, &current_height);

    // Track size changes but only recreate when we're about to render
    if ((uint32_t)current_width != ctx->last_known_width ||
        (uint32_t)current_height != ctx->last_known_height) {
        ctx->needs_swapchain_recreation = true;
        ctx->last_known_width = (uint32_t)current_width;
        ctx->last_known_height = (uint32_t)current_height;
    }

    // Only recreate if we actually need to and the size is stable
    if (ctx->needs_swapchain_recreation &&
        ((uint32_t)current_width != ctx->swapchain_extent.width ||
         (uint32_t)current_height != ctx->swapchain_extent.height)) {
        printf("Window size changed from %ux%u to %ux%u - recreating swapchain\n",
               ctx->swapchain_extent.width, ctx->swapchain_extent.height,
               current_width, current_height);
        poc_result recreate_result = recreate_swapchain(ctx);
        if (recreate_result != POC_RESULT_SUCCESS) {
            return recreate_result;
        }
        ctx->needs_swapchain_recreation = false;
    }

    // Wait for previous frame to finish
    vkWaitForFences(g_vk_state.device, 1, &ctx->in_flight_fences[ctx->current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(g_vk_state.device, 1, &ctx->in_flight_fences[ctx->current_frame]);

    // For image acquisition, we need to use a different strategy since we don't know the image index yet
    // We'll use the current frame index modulo the available semaphores
    uint32_t acquire_semaphore_index = ctx->current_frame % ctx->swapchain_image_count;

    // Acquire next image from swapchain
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(g_vk_state.device, ctx->swapchain, UINT64_MAX,
                                            ctx->image_available_semaphores[acquire_semaphore_index], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        poc_result recreate_result = recreate_swapchain(ctx);
        if (recreate_result != POC_RESULT_SUCCESS) {
            return recreate_result;
        }
        // Try acquiring again with new swapchain
        result = vkAcquireNextImageKHR(g_vk_state.device, ctx->swapchain, UINT64_MAX,
                                      ctx->image_available_semaphores[acquire_semaphore_index], VK_NULL_HANDLE, &image_index);
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("Failed to acquire swapchain image: %d\n", result);
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Reset command buffer
    vkResetCommandBuffer(ctx->command_buffers[image_index], 0);

    // Begin recording command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL
    };

    VK_CHECK(vkBeginCommandBuffer(ctx->command_buffers[image_index], &begin_info));

    // Begin render pass - clear to black if we need title bars, otherwise use normal clear color
    VkClearValue clear_values[2];

#ifdef POC_PLATFORM_LINUX
    if (needs_client_decorations(ctx->window)) {
        // Clear entire framebuffer to black first, then we'll draw the pink content area
        clear_values[0] = (VkClearValue){.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
    } else {
        clear_values[0] = (VkClearValue){.color = {{ctx->clear_color[0], ctx->clear_color[1], ctx->clear_color[2], ctx->clear_color[3]}}};
    }
#else
    clear_values[0] = (VkClearValue){.color = {{ctx->clear_color[0], ctx->clear_color[1], ctx->clear_color[2], ctx->clear_color[3]}}};
#endif
    clear_values[1] = (VkClearValue){.depthStencil = {1.0f, 0}};

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->framebuffers[image_index],
        .renderArea.offset = {0, 0},
        .renderArea.extent = ctx->swapchain_extent,
        .clearValueCount = 2,
        .pClearValues = clear_values
    };

    vkCmdBeginRenderPass(ctx->command_buffers[image_index], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind graphics pipeline
    vkCmdBindPipeline(ctx->command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphics_pipeline);

    // Render title bar if needed (client-side decorations) - after pipeline binding
    render_title_bar_if_needed(ctx, image_index);

    // Set dynamic viewport - adjust for title bar if needed
    float viewport_y = 0.0f;
    float viewport_height = (float)ctx->swapchain_extent.height;
    VkOffset2D scissor_offset = {0, 0};
    VkExtent2D scissor_extent = ctx->swapchain_extent;

#ifdef POC_PLATFORM_LINUX
    if (needs_client_decorations(ctx->window)) {
        float scale_factor = podi_window_get_scale_factor(ctx->window);
        uint32_t scaled_title_bar_height = (uint32_t)(PODI_TITLE_BAR_HEIGHT * scale_factor);

        viewport_y = (float)scaled_title_bar_height;
        viewport_height = (float)ctx->swapchain_extent.height - (float)scaled_title_bar_height;
        scissor_offset.y = scaled_title_bar_height;
        scissor_extent.height = ctx->swapchain_extent.height - scaled_title_bar_height;
    }
#endif

    VkViewport viewport = {
        .x = 0.0f,
        .y = viewport_y,
        .width = (float)ctx->swapchain_extent.width,
        .height = viewport_height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(ctx->command_buffers[image_index], 0, 1, &viewport);

    // Set dynamic scissor (unless already set by title bar rendering)
#ifdef POC_PLATFORM_LINUX
    if (!needs_client_decorations(ctx->window)) {
#endif
        VkRect2D scissor = {
            .offset = scissor_offset,
            .extent = scissor_extent
        };
        vkCmdSetScissor(ctx->command_buffers[image_index], 0, 1, &scissor);
#ifdef POC_PLATFORM_LINUX
    }
#endif

    // Render all renderables
    if (ctx->renderable_count > 0) {
        static bool first_frame = true;
        if (first_frame) {
            printf("✓ Rendering %u renderables per frame\n", ctx->renderable_count);
            first_frame = false;
        }
        for (uint32_t i = 0; i < ctx->renderable_count; i++) {
            poc_renderable *renderable = ctx->renderables[i];
            if (!renderable || renderable->vertex_buffer == VK_NULL_HANDLE || renderable->index_buffer == VK_NULL_HANDLE) {
                printf("Skipping renderable %u: invalid geometry\n", i);
                continue; // Skip renderables without valid geometry
            }

            // Removed debug output for performance

            // Update uniform buffer for this renderable
            update_renderable_uniform_buffer(renderable);

            // Bind descriptor set for this renderable
            vkCmdBindDescriptorSets(ctx->command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   ctx->pipeline_layout, 0, 1, &renderable->descriptor_set, 0, NULL);

            // Bind vertex and index buffers for this renderable
            VkBuffer vertex_buffers[] = {renderable->vertex_buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(ctx->command_buffers[image_index], 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(ctx->command_buffers[image_index], renderable->index_buffer, 0, VK_INDEX_TYPE_UINT32);

            // Draw this renderable
            vkCmdDrawIndexed(ctx->command_buffers[image_index], renderable->index_count, 1, 0, 0, 0);
        }
    }
    // DEPRECATED: Removed fallback rendering code that used shared uniform buffers
    // All rendering now uses the per-renderable system

    // End render pass
    vkCmdEndRenderPass(ctx->command_buffers[image_index]);

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(ctx->command_buffers[image_index]));

    // Store current image index and acquire semaphore index for use in end_frame
    ctx->current_image_index = image_index;
    ctx->current_acquire_semaphore_index = acquire_semaphore_index;

    return POC_RESULT_SUCCESS;
}

poc_result vulkan_context_end_frame(poc_context *ctx) {
    if (!ctx) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Get the image index from context
    uint32_t image_index = ctx->current_image_index;

    // Submit command buffer
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO
    };

    // Use the semaphore that was used for image acquisition
    VkSemaphore wait_semaphores[] = {ctx->image_available_semaphores[ctx->current_acquire_semaphore_index]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ctx->command_buffers[image_index];

    // Use the render finished semaphore for this image
    VkSemaphore signal_semaphores[] = {ctx->render_finished_semaphores[image_index]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VK_CHECK(vkQueueSubmit(g_vk_state.graphics_queue, 1, &submit_info, ctx->in_flight_fences[ctx->current_frame]));

    // Present
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR
    };

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = {ctx->swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = NULL;

    VkResult result = vkQueuePresentKHR(g_vk_state.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        poc_result recreate_result = recreate_swapchain(ctx);
        if (recreate_result != POC_RESULT_SUCCESS) {
            return recreate_result;
        }
    } else if (result != VK_SUCCESS) {
        printf("Failed to present swapchain image: %d\n", result);
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Advance to next frame
    ctx->current_frame = (ctx->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    return POC_RESULT_SUCCESS;
}

void vulkan_context_clear_color(poc_context *ctx, float r, float g, float b, float a) {
    if (!ctx) {
        return;
    }

    ctx->clear_color[0] = r;
    ctx->clear_color[1] = g;
    ctx->clear_color[2] = b;
    ctx->clear_color[3] = a;
}

poc_result vulkan_context_set_vertex_data(poc_context *ctx, poc_vertex *vertices, uint32_t vertex_count, uint32_t *indices, uint32_t index_count) {
    if (!ctx || !vertices || !indices || vertex_count == 0 || index_count == 0) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Clean up existing buffers if any
    if (ctx->vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, ctx->vertex_buffer, NULL);
        ctx->vertex_buffer = VK_NULL_HANDLE;
    }
    if (ctx->vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, ctx->vertex_buffer_memory, NULL);
        ctx->vertex_buffer_memory = VK_NULL_HANDLE;
    }
    if (ctx->index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, ctx->index_buffer, NULL);
        ctx->index_buffer = VK_NULL_HANDLE;
    }
    if (ctx->index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, ctx->index_buffer_memory, NULL);
        ctx->index_buffer_memory = VK_NULL_HANDLE;
    }

    // Create new buffers
    poc_result result = create_vertex_buffer(ctx, vertices, vertex_count);
    if (result != POC_RESULT_SUCCESS) {
        return result;
    }

    result = create_index_buffer(ctx, indices, index_count);
    if (result != POC_RESULT_SUCCESS) {
        return result;
    }

    printf("✓ Model geometry loaded: %u vertices, %u indices\n", vertex_count, index_count);
    return POC_RESULT_SUCCESS;
}

poc_result vulkan_context_load_model(poc_context *ctx, const char *obj_filename) {
    if (!ctx || !obj_filename) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    poc_model model;
    poc_obj_result obj_result = poc_model_load(obj_filename, &model);
    if (obj_result != POC_OBJ_RESULT_SUCCESS) {
        printf("Failed to load OBJ file %s: %s\n", obj_filename, poc_obj_result_to_string(obj_result));
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    printf("✓ OBJ file loaded: %u objects, %u materials\n", model.object_count, model.material_count);

    // Find the first non-empty group in any object
    poc_mesh_group *group = NULL;
    for (uint32_t obj_idx = 0; obj_idx < model.object_count && !group; obj_idx++) {
        for (uint32_t grp_idx = 0; grp_idx < model.objects[obj_idx].group_count; grp_idx++) {
            if (model.objects[obj_idx].groups[grp_idx].vertex_count > 0) {
                group = &model.objects[obj_idx].groups[grp_idx];
                break;
            }
        }
    }

    if (group) {

        poc_result result = vulkan_context_set_vertex_data(ctx, group->vertices, group->vertex_count, group->indices, group->index_count);

        poc_model_destroy(&model);
        return result;
    } else {
        printf("Warning: No geometry found in OBJ file\n");
        poc_model_destroy(&model);
        return POC_RESULT_ERROR_INIT_FAILED;
    }
}

// === New Renderable Management API ===

poc_renderable *poc_context_create_renderable(poc_context *ctx, const char *name) {
    if (!ctx) {
        return NULL;
    }

    // Resize array if needed
    if (ctx->renderable_count >= ctx->renderable_capacity) {
        uint32_t new_capacity = ctx->renderable_capacity * 2;
        poc_renderable **new_renderables = realloc(ctx->renderables, sizeof(poc_renderable*) * new_capacity);
        if (!new_renderables) {
            printf("Failed to resize renderables array\n");
            return NULL;
        }
        ctx->renderables = new_renderables;
        ctx->renderable_capacity = new_capacity;
    }

    // Allocate new renderable
    poc_renderable *renderable = malloc(sizeof(poc_renderable));
    if (!renderable) {
        printf("Failed to allocate renderable\n");
        return NULL;
    }

    // Initialize renderable
    memset(renderable, 0, sizeof(poc_renderable));
    renderable->ctx = ctx;

    // Set name
    if (name) {
        strncpy(renderable->name, name, sizeof(renderable->name) - 1);
        renderable->name[sizeof(renderable->name) - 1] = '\0';
    } else {
        snprintf(renderable->name, sizeof(renderable->name), "Renderable_%u", ctx->renderable_count);
    }

    // Initialize transform to identity matrix
    glm_mat4_identity(renderable->model_matrix);

    // Add to context
    ctx->renderables[ctx->renderable_count] = renderable;
    ctx->renderable_count++;

    printf("✓ Created renderable '%s'\n", renderable->name);
    return renderable;
}

void poc_context_destroy_renderable(poc_context *ctx, poc_renderable *renderable) {
    if (!ctx || !renderable) {
        return;
    }

    // Find renderable in array
    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < ctx->renderable_count; i++) {
        if (ctx->renderables[i] == renderable) {
            index = i;
            break;
        }
    }

    if (index == UINT32_MAX) {
        printf("Warning: Renderable not found in context\n");
        return;
    }

    // Destroy GPU resources
    if (renderable->vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, renderable->vertex_buffer, NULL);
    }
    if (renderable->vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, renderable->vertex_buffer_memory, NULL);
    }
    if (renderable->index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, renderable->index_buffer, NULL);
    }
    if (renderable->index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, renderable->index_buffer_memory, NULL);
    }

    // Destroy per-renderable uniform buffer resources
    if (renderable->uniform_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, renderable->uniform_buffer, NULL);
    }
    if (renderable->uniform_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, renderable->uniform_buffer_memory, NULL);
    }
    // Note: descriptor sets are automatically freed when the descriptor pool is destroyed

    printf("✓ Destroyed renderable '%s'\n", renderable->name);
    free(renderable);

    // Remove from array by shifting remaining elements
    for (uint32_t i = index; i < ctx->renderable_count - 1; i++) {
        ctx->renderables[i] = ctx->renderables[i + 1];
    }
    ctx->renderable_count--;
}

static poc_result create_renderable_buffers(poc_renderable *renderable, poc_vertex *vertices, uint32_t vertex_count, uint32_t *indices, uint32_t index_count) {
    if (!renderable || !vertices || !indices || vertex_count == 0 || index_count == 0) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Clean up existing buffers if any
    if (renderable->vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, renderable->vertex_buffer, NULL);
        renderable->vertex_buffer = VK_NULL_HANDLE;
    }
    if (renderable->vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, renderable->vertex_buffer_memory, NULL);
        renderable->vertex_buffer_memory = VK_NULL_HANDLE;
    }
    if (renderable->index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk_state.device, renderable->index_buffer, NULL);
        renderable->index_buffer = VK_NULL_HANDLE;
    }
    if (renderable->index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, renderable->index_buffer_memory, NULL);
        renderable->index_buffer_memory = VK_NULL_HANDLE;
    }
    if (renderable->uniform_buffer != VK_NULL_HANDLE) {
        if (renderable->uniform_buffer_mapped) {
            vkUnmapMemory(g_vk_state.device, renderable->uniform_buffer_memory);
            renderable->uniform_buffer_mapped = NULL;
        }
        vkDestroyBuffer(g_vk_state.device, renderable->uniform_buffer, NULL);
        renderable->uniform_buffer = VK_NULL_HANDLE;
    }
    if (renderable->uniform_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk_state.device, renderable->uniform_buffer_memory, NULL);
        renderable->uniform_buffer_memory = VK_NULL_HANDLE;
    }

    // Create vertex buffer
    VkDeviceSize vertex_buffer_size = sizeof(poc_vertex) * vertex_count;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    // Create staging buffer
    VkBufferCreateInfo staging_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertex_buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(g_vk_state.device, &staging_buffer_info, NULL, &staging_buffer));

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(g_vk_state.device, staging_buffer, &mem_requirements);

    VkMemoryAllocateInfo staging_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &staging_alloc_info, NULL, &staging_buffer_memory));
    vkBindBufferMemory(g_vk_state.device, staging_buffer, staging_buffer_memory, 0);

    // Copy vertex data to staging buffer
    void *data;
    vkMapMemory(g_vk_state.device, staging_buffer_memory, 0, vertex_buffer_size, 0, &data);
    memcpy(data, vertices, (size_t)vertex_buffer_size);
    vkUnmapMemory(g_vk_state.device, staging_buffer_memory);

    // Create actual vertex buffer
    VkBufferCreateInfo vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertex_buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(g_vk_state.device, &vertex_buffer_info, NULL, &renderable->vertex_buffer));

    vkGetBufferMemoryRequirements(g_vk_state.device, renderable->vertex_buffer, &mem_requirements);

    VkMemoryAllocateInfo vertex_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &vertex_alloc_info, NULL, &renderable->vertex_buffer_memory));
    vkBindBufferMemory(g_vk_state.device, renderable->vertex_buffer, renderable->vertex_buffer_memory, 0);

    // Copy from staging buffer to vertex buffer
    poc_result copy_result = copy_buffer(staging_buffer, renderable->vertex_buffer, vertex_buffer_size, renderable->ctx);
    if (copy_result != POC_RESULT_SUCCESS) {
        vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
        vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);
        return copy_result;
    }

    // Clean up staging buffer
    vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
    vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);

    // Create index buffer
    VkDeviceSize index_buffer_size = sizeof(uint32_t) * index_count;

    // Create staging buffer for indices
    VkBufferCreateInfo index_staging_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = index_buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(g_vk_state.device, &index_staging_buffer_info, NULL, &staging_buffer));

    vkGetBufferMemoryRequirements(g_vk_state.device, staging_buffer, &mem_requirements);

    VkMemoryAllocateInfo index_staging_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &index_staging_alloc_info, NULL, &staging_buffer_memory));
    vkBindBufferMemory(g_vk_state.device, staging_buffer, staging_buffer_memory, 0);

    // Copy index data to staging buffer
    vkMapMemory(g_vk_state.device, staging_buffer_memory, 0, index_buffer_size, 0, &data);
    memcpy(data, indices, (size_t)index_buffer_size);
    vkUnmapMemory(g_vk_state.device, staging_buffer_memory);

    // Create actual index buffer
    VkBufferCreateInfo index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = index_buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(g_vk_state.device, &index_buffer_info, NULL, &renderable->index_buffer));

    vkGetBufferMemoryRequirements(g_vk_state.device, renderable->index_buffer, &mem_requirements);

    VkMemoryAllocateInfo index_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &index_alloc_info, NULL, &renderable->index_buffer_memory));
    vkBindBufferMemory(g_vk_state.device, renderable->index_buffer, renderable->index_buffer_memory, 0);

    // Copy from staging buffer to index buffer
    copy_result = copy_buffer(staging_buffer, renderable->index_buffer, index_buffer_size, renderable->ctx);
    if (copy_result != POC_RESULT_SUCCESS) {
        vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
        vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);
        return copy_result;
    }

    // Clean up staging buffer
    vkDestroyBuffer(g_vk_state.device, staging_buffer, NULL);
    vkFreeMemory(g_vk_state.device, staging_buffer_memory, NULL);

    // Store counts
    renderable->vertex_count = vertex_count;
    renderable->index_count = index_count;

    // Create uniform buffer for this renderable
    VkDeviceSize uniform_buffer_size = sizeof(UniformBufferObject);

    VkBufferCreateInfo uniform_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = uniform_buffer_size,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(g_vk_state.device, &uniform_buffer_info, NULL, &renderable->uniform_buffer));

    vkGetBufferMemoryRequirements(g_vk_state.device, renderable->uniform_buffer, &mem_requirements);

    VkMemoryAllocateInfo uniform_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(g_vk_state.device, &uniform_alloc_info, NULL, &renderable->uniform_buffer_memory));
    vkBindBufferMemory(g_vk_state.device, renderable->uniform_buffer, renderable->uniform_buffer_memory, 0);

    // Map the uniform buffer memory for persistent mapping
    VK_CHECK(vkMapMemory(g_vk_state.device, renderable->uniform_buffer_memory, 0, uniform_buffer_size, 0, &renderable->uniform_buffer_mapped));

    // Allocate descriptor set for this renderable
    VkDescriptorSetAllocateInfo desc_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = renderable->ctx->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &renderable->ctx->descriptor_set_layout
    };

    VkResult desc_result = vkAllocateDescriptorSets(g_vk_state.device, &desc_alloc_info, &renderable->descriptor_set);
    if (desc_result != VK_SUCCESS) {
        printf("Failed to allocate descriptor set for renderable: %d\n", desc_result);
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Update descriptor set to point to this renderable's uniform buffer
    VkDescriptorBufferInfo buffer_info = {
        .buffer = renderable->uniform_buffer,
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };

    VkWriteDescriptorSet descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = renderable->descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo = &buffer_info
    };

    vkUpdateDescriptorSets(g_vk_state.device, 1, &descriptor_write, 0, NULL);

    return POC_RESULT_SUCCESS;
}

poc_result poc_renderable_load_model(poc_renderable *renderable, const char *obj_filename) {
    if (!renderable || !obj_filename) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    printf("Loading model '%s' into renderable '%s'\n", obj_filename, renderable->name);

    poc_model model;
    poc_obj_result obj_result = poc_model_load(obj_filename, &model);
    if (obj_result != POC_OBJ_RESULT_SUCCESS) {
        printf("Failed to load OBJ file %s: %s\n", obj_filename, poc_obj_result_to_string(obj_result));
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    printf("✓ OBJ file loaded: %u objects, %u materials\n", model.object_count, model.material_count);

    // Find the first non-empty group in any object
    poc_mesh_group *group = NULL;
    for (uint32_t obj_idx = 0; obj_idx < model.object_count && !group; obj_idx++) {
        for (uint32_t grp_idx = 0; grp_idx < model.objects[obj_idx].group_count; grp_idx++) {
            if (model.objects[obj_idx].groups[grp_idx].vertex_count > 0) {
                group = &model.objects[obj_idx].groups[grp_idx];
                break;
            }
        }
    }

    if (!group) {
        printf("Warning: No geometry found in OBJ file\n");
        poc_model_destroy(&model);
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Create GPU buffers for the renderable
    poc_result result = create_renderable_buffers(renderable, group->vertices, group->vertex_count, group->indices, group->index_count);
    if (result != POC_RESULT_SUCCESS) {
        poc_model_destroy(&model);
        return result;
    }

    // Copy material if available
    if (group->material_index < model.material_count) {
        renderable->material = model.materials[group->material_index];
        renderable->has_material = true;
        printf("✓ Material loaded: %s\n", renderable->material.name);
    } else {
        // Use default material
        renderable->has_material = false;
        printf("Using default material\n");
    }

    poc_model_destroy(&model);
    printf("✓ Model loaded into renderable '%s': %u vertices, %u indices\n",
           renderable->name, renderable->vertex_count, renderable->index_count);

    return POC_RESULT_SUCCESS;
}

void poc_renderable_set_transform(poc_renderable *renderable, mat4 transform) {
    if (!renderable) {
        return;
    }
    glm_mat4_copy(transform, renderable->model_matrix);
}

#endif