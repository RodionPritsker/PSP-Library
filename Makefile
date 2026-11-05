TARGET = psplibrary
OBJS = src/psp/main.o src/psp/ui.o src/core/core.o src/core/storage.o src/core/demo.o

PSPSDK := $(shell psp-config --pspsdk-path)
BUILD_PRX = 1
PSP_FW_VERSION = 660

INCDIR = include
CFLAGS = -O2 -G0 -Wall -Wextra -std=c99 -I$(INCDIR)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Library
PSP_EBOOT_ICON = NULL

include $(PSPSDK)/lib/build.mak
