#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include "protocol.h"
#include "clipboard.h"

struct wl_display *display;
FILE *history_file;

static struct wl_proxy *manager;
static struct wl_proxy *seat;

static void on_global(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver) {
    if (!strcmp(iface, "zwlr_data_control_manager_v1"))
        manager = (struct wl_proxy *)wl_registry_bind(
            reg, name, &zwlr_data_control_manager_v1_interface, 2
        );
    else if (!strcmp(iface, "wl_seat") && !seat)
        seat = (struct wl_proxy *)wl_registry_bind(
            reg, name, &wl_seat_interface, 1
        );
}

static void on_global_remove(void *data, struct wl_registry *reg, uint32_t name) {}

static const struct wl_registry_listener reg_listener = {
    .global        = on_global,
    .global_remove = on_global_remove,
};

void cleanup(void) {
    if (history_file) {
        fclose(history_file);
        history_file = NULL;
    }

    if (display) {
        wl_display_disconnect(display);
        display = NULL;
    }
}

int main(void) {
    atexit(cleanup);

    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "no wayland display\n");
        return 1;
    }

    history_file = fopen("/tmp/paperclip", "w");

    struct wl_registry *reg = wl_display_get_registry(display);
    wl_registry_add_listener(reg, &reg_listener, NULL);
    wl_display_roundtrip(display);

    if (!manager) {
        fprintf(stderr, "zwlr_data_control_manager_v1 not available "
                        "(needs Sway, Hyprland, or another wlroots compositor)\n");
        return 1;
    }

    struct wl_proxy *device = wl_proxy_marshal_constructor(
        manager, 1, &zwlr_data_control_device_v1_interface, NULL, seat
    );

    wl_proxy_add_listener(device, (void (**)(void))device_listener, NULL);
    wl_display_roundtrip(display);

    printf("Watching clipboard (Ctrl+C to quit)...\n");
    while (wl_display_dispatch(display) >= 0) {}

    return 0;
}
