![logo](assets/logo.png)

## How It Works

### Background: Clipboard Access in Wayland

In X11, any application can read the clipboard at any time. Wayland's security
model is stricter: a client can only interact with the clipboard while it has
keyboard focus. This prevents background processes from silently snooping on
what you copy.

However, clipboard manager tools need persistent access by design. The
[wlr-data-control](https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/blob/master/unstable/wlr-data-control-unstable-v1.xml)
protocol is a compositor-side escape hatch that grants trusted clients
(clipboard managers, screenshot tools, etc.) the ability to read and write the
clipboard without owning a visible window. It is available on wlroots-based
compositors like Sway and Hyprland.

### The Three Protocol Objects

The protocol revolves around three objects that the compositor exposes:

| Object | Role |
|---|---|
| `zwlr_data_control_manager_v1` | Singleton factory. Used to create data devices and data sources. |
| `zwlr_data_control_device_v1` | Bound to a seat. Delivers clipboard events and accepts clipboard writes. |
| `zwlr_data_control_offer_v1` | Represents a single clipboard payload. Has a list of available MIME types and a `receive` method to read the data. |

A **seat** in Wayland is a logical group of input devices — typically one
seat = one keyboard + mouse. Most desktop systems have exactly one seat.

### Skipping Code Generation

Normally you would generate C bindings from the protocol XML using
`wayland-scanner`, which produces interface structs, enums for opcodes, and
typed listener structs. Here, those are defined by hand in `main.c` using the
low-level `wl_interface` / `wl_message` structs that libwayland uses
internally. The tradeoff: no build dependency on the XML files, but opcodes
are raw integers (0, 1) instead of named constants, which is harder to read.

### Startup Sequence

```
1. wl_display_connect(NULL)
      Connect to the compositor over a Unix socket.
      NULL means use the WAYLAND_DISPLAY env var (usually "wayland-0").

2. wl_display_get_registry()  +  wl_display_roundtrip()
      The registry lists every global object the compositor offers.
      on_global() is called for each one. We look for:
        - zwlr_data_control_manager_v1  (clipboard manager)
        - wl_seat                       (input seat)

3. wl_proxy_marshal_constructor(g_manager, opcode=1, ...)
      Call get_data_device on the manager, passing the seat.
      Returns a zwlr_data_control_device_v1 bound to that seat.

4. wl_proxy_add_listener(device, device_listener, ...)
      Register our event callbacks on the device.

5. wl_display_dispatch() loop
      Process incoming events forever.
```

### Event Flow

Every time you copy something, the compositor sends a pair of events to the
data device:

```
compositor                              paperclip
    |                                       |
    |--- data_offer(new_offer) ------------>|  on_data_offer():
    |                                       |    save new_offer as g_pending_offer
    |                                       |    (destroy any previous pending offer)
    |                                       |
    |--- selection(new_offer) ------------->|  on_selection():
    |                                       |    read text from the offer
    |                                       |    print "Clipboard: ..."
    |                                       |    destroy the offer
```

**Why are there two events?**

`data_offer` comes first to deliver the offer object itself. At that point
you do not yet know if this offer will be the regular clipboard or the primary
selection (middle-click buffer). `selection` (or `primary_selection`) comes
second to say which clipboard this offer belongs to. The split also lets the
compositor batch-announce multiple MIME types on the offer before committing it.

**Primary selection** (`on_primary_selection`) is the Unix middle-click buffer.
This program ignores it — the offer is immediately destroyed without reading.

**finished** (`on_finished`) fires if the compositor destroys the data device
(e.g. the seat disappeared). The program exits.

### Reading the Clipboard Content

`on_selection()` uses a Unix pipe to transfer the data from the compositor
into the process:

```
1. pipe(fds)
      fds[0] = read end,  fds[1] = write end

2. wl_proxy_marshal(offer, opcode=0, "text/plain;charset=utf-8", fds[1])
      Send the "receive" request to the offer.
      The compositor will write the clipboard text into fds[1].

3. close(fds[1])
      Close our copy of the write end so the read will eventually EOF.

4. wl_display_flush()
      Push the receive request out to the compositor over the socket,
      since we are about to block on read() instead of dispatch().

5. read(fds[0], buf, sizeof(buf))  [loop until EOF]
      Read the text the compositor wrote.
      Print each chunk as it arrives.

6. close(fds[0])

7. offer_destroy(offer)
      Send the "destroy" request and free the proxy object.
```
7.  
The pipe approach is standard for Wayland data transfers. It avoids shared
memory and lets the compositor write asynchronously while the client reads.
