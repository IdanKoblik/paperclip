#include "protocol.h"

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
