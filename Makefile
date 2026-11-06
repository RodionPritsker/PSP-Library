TARGET = psplibrary
OBJS = src/psp/main.o src/psp/ui.o src/psp/gfx.o src/psp/text.o src/psp/image.o src/core/navigation.o src/core/core.o src/core/storage.o src/core/demo.o src/core/theme.o src/core/text_layout.o src/tracker/psp_tracker.o src/tracker/psp_fs.o

PSPSDK := $(shell psp-config --pspsdk-path)
BUILD_PRX = 1
PSP_FW_VERSION = 660

INCDIR = include
CFLAGS = -O2 -G0 -Wall -Wextra -std=c99 -I$(INCDIR)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpng -lz

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Library v0.3.1
PSP_EBOOT_ICON = NULL

include $(PSPSDK)/lib/build.mak
