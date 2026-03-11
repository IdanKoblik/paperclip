#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "clipboard.h"
#include "protocol.h"

int write_to_history(FILE *file, char *content) {
    if (!file || !content)
        return 0;

    fputs(content, file);
    fputs("\n", file);
    fflush(file);
    return 1;
}

static struct wl_proxy *pending_offer;

static void offer_destroy(struct wl_proxy *offer) {
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
        buf[n] = '\0';
        printf("Clipboard: %s\n", buf);
        fflush(stdout);

        write_to_history(history_file, buf);
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

const void *device_listener[] = {
    (void *)on_data_offer,
    (void *)on_selection,
    (void *)on_finished,
    (void *)on_primary_selection,
};
