GBDK_HOME ?= .tools/gbdk
LCC := $(GBDK_HOME)/bin/lcc

PROJECT := pogo-panic
ROM := dist/$(PROJECT).gb
WEB_ROM := public/roms/$(PROJECT).gb
SRC := src-rom/main.c src-rom/game-logic.c src-rom/tile-art.c

LCCFLAGS := -Wm-ynPOGO_PANIC -Wm-yt0x03 -Wm-yo4 -Wm-ya1
DUMP_BIN := test-artifacts/dump-levels
LEVELS_JSON := src-web/levels.json
TILE_TABLE_JSON := src-web/tile-table.json
DUMP_TILES_BIN := test-artifacts/dump-tiles
TILE_ART_JSON := src-web/tile-art.json

.PHONY: all clean check-gbdk debug

all: $(WEB_ROM) $(LEVELS_JSON) $(TILE_TABLE_JSON) $(TILE_ART_JSON)

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

$(DUMP_BIN): tools/dump-levels.c src-rom/game-logic.c src-rom/game-logic.h
	@mkdir -p test-artifacts
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tools/dump-levels.c src-rom/game-logic.c

$(LEVELS_JSON): $(DUMP_BIN)
	$(DUMP_BIN) $(TILE_TABLE_JSON) > $(LEVELS_JSON)

$(TILE_TABLE_JSON): $(LEVELS_JSON)

$(DUMP_TILES_BIN): tools/dump-tiles.c src-rom/tile-art.c src-rom/tile-art.h
	@mkdir -p test-artifacts
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tools/dump-tiles.c src-rom/tile-art.c

$(TILE_ART_JSON): $(DUMP_TILES_BIN)
	$(DUMP_TILES_BIN) > $(TILE_ART_JSON)

.PHONY: levels tile-art
levels: $(LEVELS_JSON) $(TILE_TABLE_JSON)
tile-art: $(TILE_ART_JSON)
