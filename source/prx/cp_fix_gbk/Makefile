# cp_fix_gbk — based on eleqian's TempGBA4PSP-cc
# https://github.com/lakent/TempGBA4PSP-cc (mirror of https://gitee.com/eleqian)

TARGET = cp_fix_gbk
OBJS = main.o exports.o

INCDIR = ../../SDK/include
CFLAGS = -O2 -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

LIBDIR = ../../SDK/lib
LIBS = -lpspsystemctrl_kernel

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak