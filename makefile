OS = $(shell uname -s)
CC = clang
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic -Wstrict-aliasing -Wno-unused-parameter -Wno-switch -Wno-unused-function -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Os
CFLAGS += $(shell pkg-config x11 --cflags) -DXINERAMA
LDFLAGS = -lm
LDFLAGS += $(shell pkg-config x11 --libs) -lXinerama

yawm: main.c stb_ds.c
	@$(CC) $^ $(CFLAGS) -o $@ $(LDFLAGS)

clean:
	@rm -f yawm

xephyr:
	-killall -q Xephyr
	Xephyr -screen 800x600+0+0 -screen 800x600+800+0 -ac -br +xinerama :1 2> /dev/null &

test: xephyr

testfree:
	@killall -q Xephyr

run:
	@./yawm || true

all: yawm

.PHONY: all clean
