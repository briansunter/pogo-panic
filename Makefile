GBDK_HOME ?= .tools/gbdk
LCC := $(GBDK_HOME)/bin/lcc

PROJECT := pocket-pogo-panic
ROM := dist/$(PROJECT).gb
WEB_ROM := public/roms/$(PROJECT).gb
SRC := src-rom/main.c src-rom/game-logic.c

LCCFLAGS := -Wm-ynPOGO_PANIC -Wm-yt0x03 -Wm-yo4 -Wm-ya1

.PHONY: all clean check-gbdk debug

all: $(WEB_ROM)

check-gbdk:
	@test -x "$(LCC)" || (echo "GBDK lcc not found at $(LCC). Run npm run setup:gbdk first." && exit 1)

$(ROM): $(SRC) | check-gbdk
	@mkdir -p dist
	$(LCC) $(LCCFLAGS) -o $(ROM) $(SRC)

$(WEB_ROM): $(ROM)
	@mkdir -p public/roms
	cp $(ROM) $(WEB_ROM)

clean:
	rm -f dist/* public/roms/$(PROJECT).gb

debug: LCCFLAGS += -debug -Wm-yS
debug: clean all

HOST_CC ?= cc
HOST_CFLAGS := -std=c99 -Wall -Wextra -Wno-unused-parameter -O0 -g -Isrc-rom

.PHONY: test-c
test-c:
	@mkdir -p test-artifacts
	$(HOST_CC) $(HOST_CFLAGS) -o test-artifacts/test-logic test/c/test_logic.c src-rom/game-logic.c
	./test-artifacts/test-logic
