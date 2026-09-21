# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase


# Global definitions

PSPSDK = ${shell psp-config --pspsdk-path}
PREFIX = ${shell psp-config --psp-prefix}

BUILD_PRX = 1
ENCRYPT = 1

PSP_FW_VERSION = 371
PSP_LARGE_MEMORY = 1

PSP_EBOOT_ICON = res/ICON0.png
PSP_EBOOT_TITLE = TempGBA4PSP-mod (20260807)

EXTRA_TARGETS = EBOOT.PBP

COMMON_FILE = \
 common.h \
 cpu.h memory.h video.h psp_video.h input.h sound.h main.h message.h gui.h zip.h png.h \
 ku_bridge.h exception.h ch_print.h

MAINOBJS = \
 cpu.o memory.o video_dispatch.o video_cc_wrap.o video_old_wrap.o psp_video.o input.o sound.o main.o message.o gui.o zip.o png.o \
 mips_stub.o video_blend_mips.o \
 ku_bridge.o exception.o cheats.o ch_print.o \
 boxart.o power_lifecycle.o

OBJZ = adler32.o crc32.o deflate.o infback.o inffast.o inflate.o inftrees.o trees.o zutil.o
OBJG = compress.o uncompr.o gzclose.o gzlib.o gzread.o gzwrite.o

OBJS = $(MAINOBJS) $(OBJZ) $(OBJG)

TARGET = TempGBA

VPATH = src src/zlib

CFLAGS = \
 -O2 -G0 \
 -march=allegrex -mfp32 -mgp32 -mlong32 -mabi=eabi \
 -fomit-frame-pointer -fstrict-aliasing \
 -falign-functions=32 -falign-loops -falign-labels -falign-jumps \
 -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Wmissing-prototypes -Wsign-compare \
 -DZLIB_CONST -flto

CXXFLAGS = ${CFLAGS}

ASFLAGS = ${CFLAGS}

INCDIR = SDK/include src src/zlib src/libpower
LIBDIR = SDK/lib

LIBS = src/libpower/libpsppower.a -lpspgu -lpsprtc -lpspaudio -lpspkubridge

include ${PSPSDK}/lib/build.mak

LDFLAGS += -flto

all: $(EXTRA_TARGETS)
	@$(MAKE) -C prx/exceptionHandler/prx
	@cp prx/exceptionHandler/prx/exception.prx .
	@$(MAKE) -C prx/cp_fix_gbk
	@cp prx/cp_fix_gbk/cp_fix_gbk.prx .
