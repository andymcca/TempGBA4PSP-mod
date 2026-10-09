# tempgba_me.prx - kernel module that boots the Media Engine and runs the
# "New" renderer (src/video_cc.cc) on it for the "New (ME)" video renderer.
# Ships beside EBOOT.PBP and is loaded at runtime with kuKernelLoadModule.
TARGET = tempgba_me
OBJS = me_stub.o main.o me_render_glue.o video.o

INCDIR = ../../src
CFLAGS = -O2 -G0 -Wall -march=allegrex -mfp32 -mgp32 -mlong32 -mabi=eabi
CXXFLAGS = $(CFLAGS) -DME_PRX_BUILD -fno-rtti -fno-exceptions -fno-threadsafe-statics
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

LDFLAGS = -nostartfiles
LIBS = -lpspkernel

PSP_FW_VERSION = 500

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

video.o: ../../src/video_cc.cc ../../src/me_render_env.h ../../src/me_shared.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
