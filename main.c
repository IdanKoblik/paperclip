#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

extern const struct wl_interface zwlr_data_control_manager_v1_interface;
extern const struct wl_interface zwlr_data_control_device_v1_interface;
extern const struct wl_interface zwlr_data_control_offer_v1_interface;
extern const struct wl_interface zwlr_data_control_source_v1_interface;

static const struct wl_interface *null_iface[] = { NULL };

static const struct wl_message offer_requests[] = {
    { "receive", "sh", null_iface },  /* 0 */
    { "destroy", "",   NULL       },  /* 1 */
};
static const struct wl_message offer_events[] = {
    { "offer", "s", null_iface },
};
const struct wl_interface zwlr_data_control_offer_v1_interface = {
    "zwlr_data_control_offer_v1", 1, 2, offer_requests, 1, offer_events,
};

const struct wl_interface zwlr_data_control_source_v1_interface = {
    "zwlr_data_control_source_v1", 1, 0, NULL, 0, NULL,
};

static const struct wl_interface *dev_new_id[] = { &zwlr_data_control_offer_v1_interface };
static const struct wl_interface *dev_obj[]    = { &zwlr_data_control_offer_v1_interface };
static const struct wl_interface *dev_src[]    = { &zwlr_data_control_source_v1_interface };

static const struct wl_message device_requests[] = {
    { "set_selection", "?o", dev_src },
    { "destroy",       "",   NULL    },
};
static const struct wl_message device_events[] = {
    { "data_offer",        "n",  dev_new_id },
    { "selection",         "?o", dev_obj    },
    { "finished",          "",   NULL       },
    { "primary_selection", "?o", dev_obj    },
};
const struct wl_interface zwlr_data_control_device_v1_interface = {
    "zwlr_data_control_device_v1", 2, 2, device_requests, 4, device_events,
};

static const struct wl_interface *mgr_types[] = {
    &zwlr_data_control_source_v1_interface,
    &zwlr_data_control_device_v1_interface,
    &wl_seat_interface,
};
static const struct wl_message manager_requests[] = {
    { "create_data_source", "n",  &mgr_types[0] },
    { "get_data_device",    "no", &mgr_types[1] },
    { "destroy",            "",   NULL           },
};
const struct wl_interface zwlr_data_control_manager_v1_interface = {
    "zwlr_data_control_manager_v1", 1, 3, manager_requests, 0, NULL,
};

static struct wl_display *display;
static struct wl_proxy   *manager;
static struct wl_proxy   *seat;
static struct wl_proxy   *pending_offer;

static void offer_destroy(struct wl_proxy *offer)  {
    wl_proxy_marshal(offer, 1);
    wl_proxy_destroy(offer);
}

static void on_data_offer(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
    if (pending_offer)
        offer_destroy(pending_offer);
    pending_offer = offer;
}

static void on_selection(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
    if (!offer)
        return;

    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        return;
    }

    wl_proxy_marshal(offer, 0, "text/plain;charset=utf-8", fds[1]); /* receive */
    close(fds[1]);
    wl_display_flush(display);

    char buf[4096];
    ssize_t n = read(fds[0], buf, sizeof(buf));
    if (n > 0) {
        printf("Clipboard: ");
        do { fwrite(buf, 1, n, stdout); } while ((n = read(fds[0], buf, sizeof(buf))) > 0);
        printf("\n");
        fflush(stdout);
    }
    close(fds[0]);

    offer_destroy(offer);
    pending_offer = NULL;
}

static void on_finished(void *data, struct wl_proxy *dev) {
    exit(1);
}

static void on_primary_selection(void *data, struct wl_proxy *dev, struct wl_proxy *offer) {
    if (!offer)
        return;

    if (offer == pending_offer)
        pending_offer = NULL;

    offer_destroy(offer);
}

static const void *device_listener[] = {
    (void *)on_data_offer,
    (void *)on_selection,
    (void *)on_finished,
    (void *)on_primary_selection,
};

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

int main(void) {
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "no wayland display\n");
        return 1;
    }

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
