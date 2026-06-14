TARGET = ku_bridge
OBJS = main.o sceCtrl_driver.o overclock_kern.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -march=allegrex -mfp32 -mgp32 -mlong32 -mabi=eabi
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

PSP_FW_VERSION=150

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

LIBDIR =
LDFLAGS =
LIBS = -lpsppower_driver -lpsppower

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

all: $(TARGET).prx
	psp-build-exports -s exports.exp $(TARGET).S
	@cp $(TARGET).S ../../src/$(TARGET).S
	@cp $(TARGET).prx ../../$(TARGET).prx
	@rm -f $(TARGET).S

clean:
	rm -f $(TARGET).prx $(TARGET).elf $(OBJS) exports.o PARAM.SFO EBOOT.PBP $(TARGET).S

.PHONY: all clean
