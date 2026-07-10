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
WATCOM_LIBS = user32.lib gdi32.lib

WIN32_SOURCES = \
	src/main_win32.c \
	src/platform/win32/gdi_renderer.c \
	src/script_tokenizer.c

# Keep this runtime source list in sync with scripts/build_engine_*.bat and docs/toolchain-compatibility.md.
ENGINE_RUNTIME_SOURCES = \
	src/runtime/asset_loader.c

SOURCES = $(WIN32_SOURCES) $(ENGINE_RUNTIME_SOURCES)

.PHONY: all clean print-sources

all: $(EXE)

$(EXE): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(WCL386) $(WATCOM_CFLAGS) $(WATCOM_LDFLAGS) -fe=$@ $(SOURCES) $(WATCOM_LIBS)

print-sources:
	@printf '%s\n' $(SOURCES)

clean:
	rm -rf $(BUILD_DIR)
