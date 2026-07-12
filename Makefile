# YAAT Open Watcom Win32 build manifest
#
# This manifest preserves the Windows 95-oriented compiler shape documented in
# docs/toolchain-compatibility.md: build a PE-i386 Win32 GUI executable from ANSI
# Win32 sources and link only Win95-era GUI import libs. Open Watcom C/C++ is the
# primary baseline because it can produce Windows 95 GUI binaries with a static
# Watcom runtime and without a dependency on MSVCRT.DLL.

BUILD_DIR = build
EXE = $(BUILD_DIR)/yaat.exe

WCL386 = wcl386
WATCOM_CFLAGS = -q -bt=nt -i=src -os -w3
WATCOM_LDFLAGS = -l=win95
WATCOM_LIBS = user32.lib gdi32.lib winmm.lib

WIN32_SOURCES = \
	src/main_win32.c \
	src/platform/win32/gdi_renderer.c \
	src/platform/win32/audio_winmm.c \
	src/script_tokenizer.c \
	src/script_parser.c \
	src/script_package.c \
	src/script_bytecode.c

# Keep this runtime source list in sync with scripts/build_engine_*.bat and docs/toolchain-compatibility.md.
ENGINE_RUNTIME_SOURCES = \
	src/runtime/asset_loader.c \
	src/runtime/zip_archive.c \
	src/third_party/miniz/miniz.c \
	src/third_party/miniz/miniz_zip.c \
	src/third_party/miniz/miniz_tinfl.c

SOURCES = $(WIN32_SOURCES) $(ENGINE_RUNTIME_SOURCES)

.PHONY: all clean print-sources check yaatc fixtures asset-store-smoke js-parity asset-validate

all: $(EXE)

$(EXE): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(WCL386) $(WATCOM_CFLAGS) $(WATCOM_LDFLAGS) -fe=$@ $(SOURCES) $(WATCOM_LIBS)

print-sources:
	@printf '%s\n' $(SOURCES)

clean:
	rm -rf $(BUILD_DIR)

check: fixtures asset-store-smoke js-parity asset-validate


YAATC = $(BUILD_DIR)/yaatc
YAATC_SOURCES = tools/yaatc/main.c src/script_tokenizer.c src/script_parser.c src/script_package.c src/script_bytecode.c

yaatc: $(YAATC)

$(YAATC): $(YAATC_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -Isrc -o $@ $(YAATC_SOURCES)

fixtures: $(YAATC)
	$(YAATC) tests/fixtures/scripts/two_room_key_puzzle.yaat tests/fixtures/bytecode/two_room_key_puzzle.yaatbc
asset-store-smoke: $(BUILD_DIR)/asset_store_smoke
	./$(BUILD_DIR)/asset_store_smoke

js-parity:
	node --test tests/js/parity.test.mjs

asset-validate: $(BUILD_DIR)/asset_validate
	./$(BUILD_DIR)/asset_validate game

$(BUILD_DIR)/asset_store_smoke: tests/asset_store/asset_store_smoke.c src/runtime/asset_store.c src/runtime/asset_store.h
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Isrc -o $@ tests/asset_store/asset_store_smoke.c src/runtime/asset_store.c

$(BUILD_DIR)/asset_validate: tools/asset_validate/asset_validate.c
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c99 -Wall -Wextra -pedantic -o $@ tools/asset_validate/asset_validate.c
