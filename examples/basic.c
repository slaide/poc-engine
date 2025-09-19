#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "poc_engine.h"

int my_main(podi_application *app) {
    poc_config config = (poc_config){
#ifdef POC_PLATFORM_LINUX
        .renderer_type = POC_RENDERER_VULKAN,
#endif
#ifdef POC_PLATFORM_MACOS
        .renderer_type = POC_RENDERER_METAL,
#endif
        .enable_validation = true,
        .app_name = "POC Engine Basic Example",
        .app_version = 1
    };

    poc_result result = poc_init(&config);
    if (result != POC_RESULT_SUCCESS) {
        printf("Failed to initialize POC Engine: %s\n", poc_result_to_string(result));
        return -1;
    }

    // Get display scale factor for logical sizing
    float scale_factor = podi_get_display_scale_factor(app);
    printf("Display scale factor: %.1f\n", scale_factor);

    // Create window at logical size (800x600 that appears consistent across displays)
    // Scale the requested size by the display scale factor for logical sizing
    int logical_width = 800, logical_height = 600;
    int physical_width = (int)(logical_width * scale_factor);
    int physical_height = (int)(logical_height * scale_factor);

    podi_window *window = podi_window_create(app, "POC Engine - Basic Example", physical_width, physical_height);
    if (!window) {
        printf("Failed to create window\n");
        poc_shutdown();
        return -1;
    }

    poc_context *ctx = poc_context_create(window);
    if (!ctx) {
        printf("Failed to create rendering context\n");
        podi_window_destroy(window);
        poc_shutdown();
        return -1;
    }

    const double target_fps = 120.0;

    printf("POC Engine basic example running...\n");
    printf("Running at %.0ffps, press ESC to exit\n", target_fps);
    printf("Event logging enabled - all inputs will be shown\n");
    printf("Press R/T/L to test interactive resize (bottom-right/top/left)\n");

    // Print window and scaling information
    int actual_width, actual_height;
    int framebuffer_width, framebuffer_height;
    float window_scale_factor = podi_window_get_scale_factor(window);
    podi_window_get_size(window, &actual_width, &actual_height);
    podi_window_get_framebuffer_size(window, &framebuffer_width, &framebuffer_height);

    printf("Logical size: %dx%d\n\n", logical_width, logical_height);
    printf("Window scale factor: %.1f\n", window_scale_factor);
    printf("Physical window size: %dx%d\n", actual_width, actual_height);
    printf("Framebuffer size: %dx%d\n", framebuffer_width, framebuffer_height);
    /* target frame time based on target fps */
    const double target_frame_time = 1.0 / target_fps;

    double last_frame_time = poc_get_time();
    float color_time = 0.0f;
    int frame_count = 0;

    // Resize coalescing: track current window size
    int last_width = framebuffer_width, last_height = framebuffer_height;

    while (!podi_application_should_close(app) && !podi_window_should_close(window)) {
        double current_time = poc_get_time();

        bool resize_pending = false;
        int resize_width = last_width;
        int resize_height = last_height;

        podi_event event;
        while (podi_application_poll_event(app, &event)) {
            switch (event.type) {
                case PODI_EVENT_WINDOW_CLOSE:
                    printf("WINDOW_CLOSE\n");
                    podi_window_close(window);
                    break;

                case PODI_EVENT_KEY_DOWN:
                    printf("KEY_DOWN: %s (id=%d, code=%u, mods=%s, text=%s)\n",
                           podi_get_key_name(event.key.key), event.key.key, event.key.native_keycode,
                           podi_get_modifiers_string(event.key.modifiers),
                           event.key.text ? event.key.text : "none");
                    if (event.key.key == PODI_KEY_ESCAPE) {
                        podi_window_close(window);
                    } else if (event.key.key == PODI_KEY_R) {
                        podi_window_begin_interactive_resize(window, PODI_RESIZE_EDGE_BOTTOM_RIGHT);
                    } else if (event.key.key == PODI_KEY_T) {
                        podi_window_begin_interactive_resize(window, PODI_RESIZE_EDGE_TOP);
                    } else if (event.key.key == PODI_KEY_L) {
                        podi_window_begin_interactive_resize(window, PODI_RESIZE_EDGE_LEFT);
                    }
                    break;

                case PODI_EVENT_KEY_UP:
                    printf("KEY_UP: %s (id=%d, code=%u, mods=%s)\n",
                           podi_get_key_name(event.key.key), event.key.key, event.key.native_keycode,
                           podi_get_modifiers_string(event.key.modifiers));
                    break;

                case PODI_EVENT_MOUSE_BUTTON_DOWN:
                    printf("MOUSE_DOWN: %s (id=%d)\n",
                           podi_get_mouse_button_name(event.mouse_button.button), event.mouse_button.button);
                    break;

                case PODI_EVENT_MOUSE_BUTTON_UP:
                    printf("MOUSE_UP: %s (id=%d)\n",
                           podi_get_mouse_button_name(event.mouse_button.button), event.mouse_button.button);
                    break;

                case PODI_EVENT_MOUSE_MOVE:
                    // Only print every 20th mouse move to reduce spam
                    static int mouse_move_counter = 0;
                    if (++mouse_move_counter % 20 == 0) {
                        printf("MOUSE_MOVE: (%.1f, %.1f)\n", event.mouse_move.x, event.mouse_move.y);
                    }
                    break;

                case PODI_EVENT_MOUSE_SCROLL:
                    printf("MOUSE_SCROLL: (%.2f, %.2f)\n", event.mouse_scroll.x, event.mouse_scroll.y);
                    break;

                case PODI_EVENT_WINDOW_RESIZE:
                    resize_pending = true;
                    resize_width = event.window_resize.width;
                    resize_height = event.window_resize.height;
                    break;

                case PODI_EVENT_WINDOW_FOCUS:
                    printf("WINDOW_FOCUS_GAINED\n");
                    break;

                case PODI_EVENT_WINDOW_UNFOCUS:
                    printf("WINDOW_FOCUS_LOST\n");
                    break;

                case PODI_EVENT_MOUSE_ENTER:
                    printf("MOUSE_ENTER_WINDOW\n");
                    break;

                case PODI_EVENT_MOUSE_LEAVE:
                    printf("MOUSE_LEAVE_WINDOW\n");
                    break;

                default:
                    printf("UNKNOWN_EVENT: type=%d\n", event.type);
                    break;
            }
        }

        if (resize_pending) {
            printf("WINDOW_RESIZE: %dx%d\n", resize_width, resize_height);
            last_width = resize_width;
            last_height = resize_height;
        }

        // Frame rate control
        double frame_elapsed = current_time - last_frame_time;

        double remaining_frame_time=target_frame_time - frame_elapsed;
        if (remaining_frame_time>0) {
            poc_sleep(remaining_frame_time);
        }

        color_time += (float)target_frame_time;
        float r = (sinf(color_time) + 1.0f) * 0.5f;
        float g = (sinf(color_time + 2.0f) + 1.0f) * 0.5f;
        float b = (sinf(color_time + 4.0f) + 1.0f) * 0.5f;

        result = poc_context_begin_frame(ctx);
        if (result == POC_RESULT_SUCCESS) {
            poc_context_clear_color(ctx, r, g, b, 1.0f);
            result = poc_context_end_frame(ctx);
            if (result != POC_RESULT_SUCCESS) {
                printf("Failed to end frame: %s\n", poc_result_to_string(result));
                break;
            }
        } else {
            printf("Failed to begin frame: %s\n", poc_result_to_string(result));
            break;
        }

        last_frame_time = current_time;
        frame_count++;
    }

    poc_context_destroy(ctx);
    podi_window_destroy(window);
    poc_shutdown();

    printf("POC Engine basic example finished\n");
    return 0;
}

int main(void) {
    return podi_main(my_main);
}
