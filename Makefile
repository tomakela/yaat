# YAAT Win32 build manifest
#
# This manifest is intended to preserve the Windows 95-oriented compiler shape
# documented in docs/toolchain-compatibility.md: build a PE-i386 Win32 GUI
# executable from ANSI Win32 sources and link only Win95-era GUI import libs.
# It defaults to the old MinGW.org-style command from that document, but callers
# may override CC, CFLAGS, LDFLAGS, EXE, or ENGINE_RUNTIME_SOURCES for another
# validated Win95-capable compiler.

BUILD_DIR = build
EXE = $(BUILD_DIR)/yaat.exe

CC = mingw32-gcc
CFLAGS = -I src -march=i386 -Os
LDFLAGS = -mwindows -static-libgcc
LDLIBS = -luser32 -lgdi32

WIN32_SOURCES = \
	src/main_win32.c \
	src/platform/win32/gdi_renderer.c \
	src/script_tokenizer.c

# Add future engine runtime .c files here.
ENGINE_RUNTIME_SOURCES = \
	src/runtime/asset_loader.c

SOURCES = $(WIN32_SOURCES) $(ENGINE_RUNTIME_SOURCES)

.PHONY: all clean print-sources

all: $(EXE)

$(EXE): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)

print-sources:
	@printf '%s\n' $(SOURCES)

clean:
	rm -rf $(BUILD_DIR)
