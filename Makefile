CC      ?= gcc
CFLAGS  += -O3 -march=native -flto -Wall -Wextra -Werror -fPIC $(shell pkg-config --cflags gtk+-3.0 libxfce4panel-2.0 libxfce4util-1.0 libxfce4ui-2)
LDFLAGS += -shared -flto -Wl,-O1,--as-needed $(shell pkg-config --libs gtk+-3.0 libxfce4panel-2.0 libxfce4util-1.0 libxfce4ui-2)

SRCS        = clock.c prefs.c
HDRS        = clock.h
PLUGIN_DIR  = /usr/lib/xfce4/panel/plugins
DESKTOP_DIR = /usr/share/xfce4/panel/plugins

all: libxfce4-clock.so

libxfce4-clock.so: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

install: all
	install -Dm755 libxfce4-clock.so $(DESTDIR)$(PLUGIN_DIR)/libxfce4-clock.so
	install -Dm644 xfce4-clock.desktop $(DESTDIR)$(DESKTOP_DIR)/xfce4-clock.desktop

clean:
	rm -f libxfce4-clock.so
