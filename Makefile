CC      = cc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lwayland-client

TARGET  = paperclip
SRC     = main.c clipboard.c protocol.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: clean
