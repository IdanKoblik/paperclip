#pragma once
#include <stdio.h>
#include <wayland-client.h>

extern struct wl_display *display;
extern FILE *history_file;

extern const void *device_listener[];

int write_to_history(FILE *file, char *content);
