#define _POSIX_C_SOURCE 199309L
#include "poc_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

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

    podi_window *window = podi_window_create(app, "POC Engine - Basic Example", 800, 600);
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

    printf("POC Engine basic example running...\n");
    printf("Running for 4 seconds at 30fps, or press ESC to exit\n");
    printf("Event logging enabled - all inputs will be shown\n\n");

    const double target_fps = 30.0;
    const double frame_time = 1.0 / target_fps;
    const double max_runtime = 4.0; // 4 seconds

    struct timespec start_time, current_time, last_frame_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    last_frame_time = start_time;

    float color_time = 0.0f;
    int frame_count = 0;

    while (!podi_application_should_close(app) && !podi_window_should_close(window)) {
        // Check if 4 seconds have elapsed
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double elapsed = (current_time.tv_sec - start_time.tv_sec) +
                        (current_time.tv_nsec - start_time.tv_nsec) / 1e9;

        if (elapsed >= max_runtime) {
            printf("Auto-exiting after %.1f seconds (%d frames)\n", elapsed, frame_count);
            break;
        }
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
                    printf("WINDOW_RESIZE: %dx%d\n", event.window_resize.width, event.window_resize.height);
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

        // Frame rate control
        double frame_elapsed = (current_time.tv_sec - last_frame_time.tv_sec) +
                              (current_time.tv_nsec - last_frame_time.tv_nsec) / 1e9;

        if (frame_elapsed >= frame_time) {
            color_time += (float)frame_elapsed;
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
        } else {
            // Sleep for a short time to avoid busy waiting
            struct timespec sleep_time = {
                .tv_sec = 0,
                .tv_nsec = 1000000 // 1ms
            };
            nanosleep(&sleep_time, NULL);
        }
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