# Makefile for NVDAMaxwell.kext & maxwell_info (macOS 14+ x86_64)
# Run on macOS with Xcode / Command Line Tools installed.

KEXT_NAME       := NVDAMaxwell
KEXT_BUNDLE     := $(KEXT_NAME).kext
KEXT_BINARY     := $(KEXT_BUNDLE)/Contents/MacOS/$(KEXT_NAME)
KEXT_PLIST      := $(KEXT_BUNDLE)/Contents/Info.plist

SDKROOT         := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
CXX             := clang++
CC              := clang

CXXFLAGS := \
	-target x86_64-apple-macos14.0 \
	-isysroot $(SDKROOT) \
	-I$(SDKROOT)/System/Library/Frameworks/Kernel.framework/Headers \
	-I../kext/include \
	-Wall -Wextra -O2 \
	-mkernel -fno-builtin -fno-common -fno-exceptions -fno-rtti \
	-mno-red-zone -mcmodel=kernel

LDFLAGS := \
	-target x86_64-apple-macos14.0 \
	-isysroot $(SDKROOT) \
	-Wl,-kext -nostdlib -lkmodc++ -lkmod -lcc_kext

.PHONY: all kext tool clean

all: kext tool

kext: $(KEXT_BINARY)

KEXT_SRCS := \
	../kext/src/MaxwellProbe.cpp \
	../kext/src/MaxwellBiosI2C.cpp \
	../kext/src/MaxwellEvoDisp.cpp \
	../kext/src/MaxwellFramebuffer.cpp

KEXT_HDRS := \
	../kext/include/gm204_regs.h \
	../kext/include/nvkm_osx_shim.h \
	../kext/include/MaxwellBiosI2C.h \
	../kext/include/MaxwellEvoDisp.h \
	../kext/src/MaxwellProbe.hpp \
	../kext/src/MaxwellFramebuffer.hpp

$(KEXT_BINARY): $(KEXT_SRCS) $(KEXT_HDRS) ../kext/Info.plist
	mkdir -p $(KEXT_BUNDLE)/Contents/MacOS
	cp ../kext/Info.plist $(KEXT_PLIST)
	$(CXX) $(CXXFLAGS) $(KEXT_SRCS) $(LDFLAGS) -o $(KEXT_BINARY)
	codesign --force --sign - $(KEXT_BUNDLE) || true

tool: maxwell_info

maxwell_info: ../tools/maxwell_info.c
	$(CC) -target x86_64-apple-macos14.0 -Wall -O2 ../tools/maxwell_info.c \
		-framework IOKit -framework CoreFoundation -o maxwell_info

clean:
	rm -rf $(KEXT_BUNDLE) maxwell_info
