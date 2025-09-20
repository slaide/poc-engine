#ifdef POC_PLATFORM_LINUX

#include "vulkan_renderer.h"
#include "poc_engine.h"
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Include Vulkan platform-specific headers after X11 types are defined
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>

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
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer *framebuffers;
    uint32_t framebuffer_count;  // Track framebuffer count independently from swapchain
    VkShaderModule vert_shader_module;
    VkShaderModule frag_shader_module;
};

static vulkan_state g_vk_state = {0};

static const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};


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
    printf("  Format: %d\n", ctx->swapchain_format);
    printf("  Extent: %ux%u\n", ctx->swapchain_extent.width, ctx->swapchain_extent.height);
    printf("  Image count: %u\n", ctx->swapchain_image_count);
    printf("  Present mode: %d\n", present_mode);

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
            ctx->swapchain_image_views[i]
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx->render_pass,
            .attachmentCount = 1,
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

static poc_result create_render_pass(poc_context *ctx) {
    VkAttachmentDescription color_attachment = {
        .format = ctx->swapchain_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(g_vk_state.device, &render_pass_info, NULL, &ctx->render_pass));

    printf("✓ Render pass created\n");
    return POC_RESULT_SUCCESS;
}

static poc_result create_graphics_pipeline(poc_context *ctx) {
    // Load shaders
    ctx->vert_shader_module = create_shader_module("shaders/triangle.vert.spv");
    ctx->frag_shader_module = create_shader_module("shaders/triangle.frag.spv");

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

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)ctx->swapchain_extent.width,
        .height = (float)ctx->swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = ctx->swapchain_extent
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
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

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = NULL,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    VK_CHECK(vkCreatePipelineLayout(g_vk_state.device, &pipeline_layout_info, NULL, &ctx->pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = NULL,
        .pColorBlendState = &color_blending,
        .pDynamicState = NULL,
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

    // Initialize resize tracking
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

poc_result vulkan_context_begin_frame(poc_context *ctx) {
    if (!ctx) {
        return POC_RESULT_ERROR_INIT_FAILED;
    }

    // Check if window size has changed
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

    // Begin render pass
    VkClearValue clear_color = {{{ctx->clear_color[0], ctx->clear_color[1], ctx->clear_color[2], ctx->clear_color[3]}}};

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->framebuffers[image_index],
        .renderArea.offset = {0, 0},
        .renderArea.extent = ctx->swapchain_extent,
        .clearValueCount = 1,
        .pClearValues = &clear_color
    };

    vkCmdBeginRenderPass(ctx->command_buffers[image_index], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind graphics pipeline
    vkCmdBindPipeline(ctx->command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphics_pipeline);

    // Draw triangle (3 vertices, 1 instance, no vertex buffers needed as vertices are hardcoded in shader)
    vkCmdDraw(ctx->command_buffers[image_index], 3, 1, 0, 0);

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

#endif