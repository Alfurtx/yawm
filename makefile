#
# Yet Another Window Manager
#

OS = $(shell uname -s)
CC = clang
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic -Wstrict-aliasing
CFLAGS += -Wno-unused-parameter -Wno-switch -Wno-unused-function
LDFLAGS = -lm -lX11

# X11 flags (based on dwm)
# X11INC = /usr/X11R6/include
# X11LIB = /usr/X11R6/lib
# INCS = -I$(X11INC)
# LIBS = -L$(X11LIB)

CFLAGS += -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS += -Os
LDFLAGS += $(LIBS)

SRC = $(wildcard ./*.c)
OBJ = $(SRC:.c=.o)
BIN = .
TARGET = $(BIN)/yawm

.PHONY: all clean

all: update_ctags yawm

# Remove when release, but until then, I am too lazy to write something like 'make debug'
update_ctags:
	@ctags -R --exclude=.git --exclude=.vscode

%.o: %.c
	$(CC) -c $(CFLAGS) $<

yawm: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

test: xephyr
	DISPLAY=":1" && $(TARGET)

xephyr:
	Xephyr -br -ac -noreset -screen 800x600 :1 2> /dev/null &

testfree:
	killall -q Xephyr

run:
	@$(TARGET) || true
