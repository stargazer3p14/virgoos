#
#	SeptemberOS Operating System main makefile.
#

# Configuration options/definitions
ifeq ($(ARCH),)
ARCH=x86
#ARCH=arm
#ARCH=mips
endif

ifeq ($(MACH),)
#MACH=""
MACH=pc
#MACH=versatile
#MACH=evmdm6467
#MACH=malta
endif


#
# SeptemberOS core modules
#

# Pthreads support
CFG_PTHREADS = 1
# POSIX I/O support
CFG_POSIXIO = 1
# POSIX processes emulation support
CFG_POSIXPROC = 1
# Memory manager
CFG_MEMMAN = 1
# Task manager
CFG_TASKMAN = 1
# Device manager
CFG_DEVMAN = 1
# Timers
CFG_TIMERS = 1

#
# Filesystems
#
ifneq ($(CFG_POSIXIO),)
# ext2
CFG_FS_EXT2 = 1
# FAT
CFG_FS_FAT = 1
endif

#
# Network Support Library (depends on CFG_POSIXIO)
#
ifneq ($(CFG_POSIXIO),)
CFG_NETWORK = 1
endif

#
# Standard C Support Library (depends on CFG_POSIXIO)
#
CFG_LIBC = 1

#
# Device drivers
#
ifneq ($(CFG_DEVMAN),)

# IDE disk
ifeq ($(MACH), pc)
CFG_IDE = 1
endif
ifeq ($(MACH), evmdm6467)
CFG_IDE = 1
endif
# PCI host
ifneq ($(MACH), evmdm6467)	# DM6467 doesn't have a PCI host
ifneq ($(MACH), versatile)	# versatile doesn't have a PCI host	
CFG_PCIHOST = 1
endif # versatile
endif # evmdm6467
# USB EHCI host (incomplete - don't use)
# As of beta release 1.0 this is non-functional
#ifeq ($(MACH), pc)
#CFG_EHCI_USBHOST = 1
#endif
# PC keyboard
ifeq ($(MACH), pc)
CFG_KEYBOARD = 1
endif

# NICs
ifneq ($(CFG_NETWORK),)
# DM6467 EMAC
ifeq ($(MACH), evmdm6467)
# DM6467 doesn't have a PCI host
CFG_NIC_DM646x_EMAC = 1
else # evmdm6467
ifeq ($(MACH), versatile)
# Beta release 1.0: SMSC91C111 driver is not functional yet, at least with QEMU
#CFG_NIC_SMSC91C111 = 1
else
# Intel 8255x (eepro)
#CFG_NIC_8255x = 1
# AMD pcnet32
CFG_NIC_PCNET32 = 1
endif # versatile
endif # evmdm6467
endif # CFG_NETWORK

# UARTs
CFG_UART = 1

ifneq ($(CFG_UART),)
ifeq ($(MACH), versatile)
CFG_UART_PL011 = 1
else
CFG_UART_16x50 = 1
endif
endif

# I2C
CFG_I2C = 1

ifneq ($(CFG_I2C),)
ifeq ($(MACH), evmdm6467)
CFG_DM646x_I2C = 1
endif
endif

# Video devices (encoders and decoders)
# As of beta release 1.0 Video drivers are not functional yet
#CFG_VIDEO = 1
#
#ifneq ($(CFG_VIDEO),)
#ifeq ($(MACH), evmdm6467)
#CFG_DM646x_VPIF = 1
#CFG_VIDEO_DEC_TVP7002 = 1
#endif
#endif


# VGA text mode terminal
ifeq ($(MACH), pc)
CFG_TERMINAL = 1
endif

endif	# CFG_DEVMAN

BASE_DIR = $(PWD)

SOSBASE_DIR = $(BASE_DIR)/base
LIBC_DIR = $(BASE_DIR)/libc
INC_DIR = $(BASE_DIR)/include
DRV_DIR = $(BASE_DIR)/drivers
NET_DIR = $(BASE_DIR)/network
FS_DIR = $(BASE_DIR)/fs
POSIX_DIR = $(BASE_DIR)/posix
APP_DIR = $(BASE_DIR)/samples

SEPTOS_DIR = $(BASE_DIR)/..
TOOLS_DIR = $(SEPTOS_DIR)/tools

# You may modify those to be architecture and machine specific
OBJ_DIR = $(BASE_DIR)/obj
IMAGE_DIR = $(BASE_DIR)/image

ifeq ($(ARCH), x86)
#BUILD_TOOLS_PREFIX = i686-pc-linux-gnu
endif

ifeq ($(ARCH), arm)
ifeq ($(MACH), versatile)
BUILD_TOOLS_PREFIX = arm-none-eabi
endif
ifeq ($(MACH), evmdm6467)
BUILD_TOOLS_PREFIX = arm-none-eabi
endif
endif

ifeq ($(ARCH), mips)
BUILD_TOOLS_PREFIX = mips-sde-elf
endif


CC = $(BUILD_TOOLS_PREFIX)-gcc
AS = $(BUILD_TOOLS_PREFIX)-as
ifneq ($(ARCH), x86)
LD = $(BUILD_TOOLS_PREFIX)-ld
OBJCOPY = $(BUILD_TOOLS_PREFIX)-objcopy
else
# x86 evaluation variant needs special tools for PE target
LD = $(TOOLS_DIR)/bin/i586-pc-septos-ld
OBJCOPY = $(TOOLS_DIR)/bin/i586-pc-septos-objcopy
ASM = nasm
endif


# Set BUILD_TOOLS_DIR to a directory where your toolchain is installed
# If tools for different targets reside in different places, you may set this with `ifeq ($(ARCH), xxx) / endif' conditions
ifeq ($(ARCH), x86)
BUILD_TOOLS_DIR = $(BASE_DIR)/../../../x86-tools
GCC_INC_DIR = $(BUILD_TOOLS_DIR)/lib/gcc/i586-pc-septos/4.2.1/include
GCC_LIB_DIR = $(BUILD_TOOLS_DIR)/lib/gcc/i586-pc-septos/4.2.1
endif
ifeq ($(ARCH), arm)
BUILD_TOOLS_DIR = $(BASE_DIR)/../../../arm-tools
GCC_INC_DIR = $(BUILD_TOOLS_DIR)/lib/gcc/$(BUILD_TOOLS_PREFIX)/4.4.1/include
GCC_LIB_DIR = $(BUILD_TOOLS_DIR)/lib/gcc/$(BUILD_TOOLS_PREFIX)/4.4.1
endif
ifeq ($(ARCH), mips)
BUILD_TOOLS_DIR = $(BASE_DIR)/../../../mips-tools
GCC_INC_DIR = $(BUILD_TOOLS_DIR)/lib/gcc/$(BUILD_TOOLS_PREFIX)/4.4.1/include
GCC_LIB_DIR = $(BUILD_TOOLS_DIR)/lib/gcc/$(BUILD_TOOLS_PREFIX)/4.4.1
endif

# Set arch/ and mach/ sub-dirs correctly
ifneq ($(MACH),)
ARCH_DIR=$(BASE_DIR)/arch/$(ARCH)/mach-$(MACH)
else
ARCH_DIR=$(BASE_DIR)/arch/$(ARCH)
endif
ARCH_INC_DIR=$(ARCH_DIR)/include

# Bootsector (x86 evaluation specific)
ifeq ($(ARCH), x86)
BOOTSEC_DIR = $(ARCH_DIR)/bootsec
BOOTSEC = $(BOOTSEC_DIR)/bootsec.bin
endif

SEPTOS_DEF = $(INC_DIR)/sosdef.h

# Separate sets of drivers object files, specific for different targets
DRV_OBJS = 
ifneq ($(CFG_IDE),)
DRV_OBJS += $(OBJ_DIR)/ide.obj
endif
ifneq ($(CFG_PCIHOST),)
DRV_OBJS += $(OBJ_DIR)/pcihost.obj
endif
ifneq ($(CFG_EHCI_USBHOST),)
DRV_OBJS += $(OBJ_DIR)/ehci.obj
endif
ifneq ($(CFG_KEYBOARD),)
DRV_OBJS += $(OBJ_DIR)/keyboard.obj
endif
ifneq ($(CFG_NIC_DM646x_EMAC),)
DRV_OBJS += $(OBJ_DIR)/dm646x-emac.obj
endif
ifneq ($(CFG_NIC_8255x),)
DRV_OBJS += $(OBJ_DIR)/8255x.obj
endif
ifneq ($(CFG_NIC_PCNET32),)
DRV_OBJS += $(OBJ_DIR)/am79970.obj
endif
ifneq ($(CFG_NIC_SMSC91C111),)
DRV_OBJS += $(OBJ_DIR)/smsc91c111.obj
endif
ifneq ($(CFG_UART_PL011),)
DRV_OBJS += $(OBJ_DIR)/pl011.obj
endif
ifneq ($(CFG_UART_16x50),)
DRV_OBJS += $(OBJ_DIR)/16x50.obj
endif
ifneq ($(CFG_DM646x_I2C),)
DRV_OBJS += $(OBJ_DIR)/dm646x-i2c.obj
endif
ifneq ($(CFG_DM646x_VPIF),)
DRV_OBJS += $(OBJ_DIR)/dm646x-vpif.obj
endif
ifneq ($(CFG_VIDEO_DEC_TVP7002),)
DRV_OBJS += $(OBJ_DIR)/tvp7002.obj
endif
ifneq ($(CFG_TERMINAL),)
DRV_OBJS += $(OBJ_DIR)/terminal.obj
endif


# Network Support Library Objects
ifneq ($(CFG_NETWORK),)
NET_OBJS = $(OBJ_DIR)/socket.obj $(OBJ_DIR)/arp.obj $(OBJ_DIR)/ip.obj $(OBJ_DIR)/tcp.obj $(OBJ_DIR)/udp.obj $(OBJ_DIR)/icmp.obj $(OBJ_DIR)/ethernet.obj $(OBJ_DIR)/dhcp.obj
else
NET_OBJS =
endif

# Filesystem objects
FS_OBJS =
ifneq ($(CFG_FS_EXT2),)
FS_OBJS += $(OBJ_DIR)/ext2.obj
endif
ifneq ($(CFG_FS_FAT),)
FS_OBJS += $(OBJ_DIR)/fat.obj
endif

# This should always be in - OS startup
SEPTOS_OBJS = $(OBJ_DIR)/sosbasic.obj $(OBJ_DIR)/sosbasic-arch.obj
# Base components objects (init.obj is handled differently for x86 and other platforms)
ifeq ($(ARCH), x86)
SEPTOS_OBJS += $(OBJ_DIR)/init.obj
endif
ifneq ($(CFG_MEMMAN),)
SEPTOS_OBJS += $(OBJ_DIR)/memman.obj
endif
ifneq ($(CFG_TASKMAN),)
SEPTOS_OBJS += $(OBJ_DIR)/taskman.obj $(OBJ_DIR)/tasks-arch.obj
endif
ifneq ($(CFG_POSIXIO),)
SEPTOS_OBJS += $(OBJ_DIR)/io.obj
endif
ifneq ($(CFG_POSIXPROC),)
SEPTOS_OBJS += $(OBJ_DIR)/exec.obj
endif
ifneq ($(CFG_DEVMAN),)
SEPTOS_OBJS += $(OBJ_DIR)/devman.obj
endif
ifneq ($(CFG_TIMERS),)
SEPTOS_OBJS += $(OBJ_DIR)/timers.obj $(OBJ_DIR)/timers-arch.obj
endif

# Pthreads support (optional at build level)
ifneq ($(CFG_PTHREADS),)
SEPTOS_OBJS += $(OBJ_DIR)/pthread.obj
endif

# Standard C library support object
ifneq ($(CFG_LIBC),)
LIBC_OBJS = $(OBJ_DIR)/libc.obj
else
LIBC_OBJS =
endif

ifeq ($(ARCH), x86)
IMAGE = $(IMAGE_DIR)/image.pe
BOOT_IMAGE = $(IMAGE_DIR)/boot.flp
endif
ifeq ($(ARCH), arm)
IMAGE = $(IMAGE_DIR)/image.elf
BOOT_IMAGE = $(IMAGE_DIR)/image.elf
endif
ifeq ($(ARCH), mips)
IMAGE = $(IMAGE_DIR)/image.elf
BOOT_IMAGE = $(IMAGE_DIR)/image.elf
endif

#
# Compiler options (arch and mach specific)
#

# x86
ifeq ($(ARCH), x86)
C_OPT = -c -masm=intel -I$(INC_DIR) -I$(ARCH_INC_DIR) -I$(GCC_INC_DIR) -std=c99 -D$(ARCH) -D$(MACH) -ffreestanding -fleading-underscore 
A_OPT = -I$(INC_DIR)
L_OPT = -e _init --image-base 0x100000 --subsystem native
endif

# arm
ifeq ($(ARCH), arm)
ifeq ($(MACH), versatile)
C_OPT = -c -mcpu=arm926ej-s -g -I$(INC_DIR) -I$(ARCH_INC_DIR) -I$(GCC_INC_DIR) -std=c99 -ffreestanding -mlittle-endian -D$(ARCH) -D$(MACH)
endif
ifeq ($(MACH), evmdm6467)
C_OPT = -c -mcpu=arm926ej-s -I$(INC_DIR) -I$(ARCH_INC_DIR) -I$(GCC_INC_DIR) -std=c99 -ffreestanding -mlittle-endian -D$(ARCH) -D$(MACH)
endif
L_OPT = -T $(ARCH_DIR)/septos.ld 
endif

# mips
ifeq ($(ARCH), mips)
C_OPT = -c -march=24kf -G0 -I$(INC_DIR) -I$(ARCH_INC_DIR) -I$(GCC_INC_DIR) -std=c99 -ffreestanding -D$(ARCH) -D$(MACH)
A_OPT = -march=24kf -g
L_OPT = -T $(ARCH_DIR)/septos.ld 
endif


########################################################################
# Record configuration of this makefile in $(INC_DIR)/_config.h for code reference
########################################################################
config:	$(INC_DIR)/_config.h

$(INC_DIR)/_config.h:
	@echo "/* Automatically generated SeptemberOS _config.h */" > $(INC_DIR)/_config.h
	@echo "#ifndef _CONFIG__H" >> $(INC_DIR)/_config.h
	@echo "#define _CONFIG__H" >> $(INC_DIR)/_config.h
	@echo "" >> $(INC_DIR)/_config.h
ifneq ($(CFG_PTHREADS),)
	@echo "#define	CFG_PTHREADS	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_POSIXIO),)
	@echo "#define	CFG_POSIXIO	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_POSIXPROC),)
	@echo "#define	CFG_POSIXPROC	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_MEMMAN),)
	@echo "#define	CFG_MEMMAN	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_TASKMAN),)
	@echo "#define	CFG_TASKMAN	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_DEVMAN),)
	@echo "#define	CFG_DEVMAN	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_TIMERS),)
	@echo "#define	CFG_TIMERS	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_FS_EXT2),)
	@echo "#define	CFG_FS_EXT2	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_FS_FAT),)
	@echo "#define	CFG_FS_FAT	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_NETWORK),)
	@echo "#define	CFG_NETWORK	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_LIBC),)
	@echo "#define	CFG_LIBC	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_IDE),)
	@echo "#define	CFG_IDE	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_PCIHOST),)
	@echo "#define	CFG_PCIHOST	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_EHCI_USBHOST),)
	@echo "#define	CFG_EHCI_USBHOST	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_KEYBOARD),)
	@echo "#define	CFG_KEYBOARD	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_NIC_DM646x_EMAC),)
	@echo "#define	CFG_NIC_DM646x_EMAC	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_NIC_8255x),)
	@echo "#define	CFG_NIC_8255x	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_NIC_PCNET32),)
	@echo "#define	CFG_NIC_PCNET32	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_NIC_SMSC91C111),)
	@echo "#define	CFG_NIC_SMSC91C111	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_UART_PL011),)
	@echo "#define	CFG_UART_PL011	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_UART_16x50),)
	@echo "#define	CFG_UART_16x50	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_DM646x_I2C),)
	@echo "#define	CFG_DM646x_I2C	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_DM646x_VPIF),)
	@echo "#define  CFG_DM646x_VPIF	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_VIDEO),)
	@echo "#define  CFG_VIDEO" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_VIDEO_DEC_TVP7002),)
	@echo "#define  CFG_VIDEO_DEC_TVP7002	1" >> $(INC_DIR)/_config.h
endif
ifneq ($(CFG_TERMINAL),)
	@echo "#define	CFG_TERMINAL	1" >> $(INC_DIR)/_config.h
endif
	@echo "" >> $(INC_DIR)/_config.h
	@echo "#endif // _CONFIG__H" >> $(INC_DIR)/_config.h
	@echo "" >> $(INC_DIR)/_config.h


# Target for `make clean'
clean:
	rm -rf $(BOOTSEC) $(IMAGE) $(APP_OBJS) $(LIBC_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(NET_OBJS) $(FS_OBJS) $(OBJ_DIR)/* $(IMAGE_DIR)/* $(INC_DIR)/_config.h

########################################################################
#	Image targets (per architecture)
########################################################################

ifeq ($(ARCH), x86)
bootsec:	$(BOOTSEC_DIR)/bootsec.bin
all:
	PATH=$(PATH):$(BUILD_TOOLS_DIR)/bin make disk
endif
ifeq ($(ARCH), arm)
all:
	PATH=$(PATH):$(BUILD_TOOLS_DIR)/bin make image
endif
ifeq ($(ARCH), mips)
all:
	PATH=$(PATH):$(BUILD_TOOLS_DIR)/bin make image
endif

image:	config $(IMAGE)
	echo Image = '$(IMAGE)'

disk:	image
	i=`wc -c ./image/image.pe | sed -r s/"[^0123456789]"\*//g` ;\
	echo Image size = $$i ;\
	j=$$(( $$i / 512 + 1 )) ;\
	echo Sectors number = $$j ;\
	sed -r s/bsImageSize\.\*DW\.\*/bsImageSize\ DW\ $$j\ \;\ 0x27/g $(BOOTSEC_DIR)/bootsec.asm.sav > $(BOOTSEC_DIR)/bootsec.asm ;\
	make bootsec ;\
	rm -rf image/boot0.flp ;\
	dd if=$(BOOTSEC_DIR)/bootsec.bin of=$(IMAGE_DIR)/boot0.flp ;\
	dd if=$(IMAGE_DIR)/image.pe of=$(IMAGE_DIR)/boot0.flp bs=1 seek=512 skip=0 oflag=append conv=notrunc
	cp $(IMAGE_DIR)/boot0.flp $(BOOT_IMAGE)


########################################################################
#	Sample applications targets
########################################################################

fltctrl:
	APP_OBJS=$(OBJ_DIR)/fltctrl.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
sample:
	APP_OBJS=$(OBJ_DIR)/sample.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
term-mon: 
	APP_OBJS="$(OBJ_DIR)/monitor.obj $(OBJ_DIR)/term-mon.obj" make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
uart-mon: 
	APP_OBJS="$(OBJ_DIR)/monitor.obj $(OBJ_DIR)/uart-mon.obj" make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
telnet-mon: 
	APP_OBJS="$(OBJ_DIR)/monitor.obj $(OBJ_DIR)/telnet-mon.obj" make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
uart:
	APP_OBJS=$(OBJ_DIR)/uart.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
uart-sample:
	APP_OBJS=$(OBJ_DIR)/uart-sample.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
udp-sample:
	APP_OBJS=$(OBJ_DIR)/udp-sample.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
tcp-srv:
	APP_OBJS=$(OBJ_DIR)/tcp-srv.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
tcp-client:
	APP_OBJS=$(OBJ_DIR)/tcp-client.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
telnet-srv:
	APP_OBJS=$(OBJ_DIR)/telnet-srv.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
http-srv:
	APP_OBJS=$(OBJ_DIR)/http-srv.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
tcp-srv-mt:
	APP_OBJS=$(OBJ_DIR)/tcp-srv-mt.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
socktests:
	APP_OBJS=$(OBJ_DIR)/socktests.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
video-sample:
	APP_OBJS=$(OBJ_DIR)/video-sample.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
	
exec-sample:
	APP_OBJS=$(OBJ_DIR)/exec-sample.obj make all
	cp $(BOOT_IMAGE) $(IMAGE_DIR)/$@
	

########################################################################
#	Sample applications' objects
########################################################################

$(OBJ_DIR)/fltctrl.obj:	$(APP_DIR)/fltctrl/fltctrl.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/fltctrl.obj $(APP_DIR)/fltctrl/fltctrl.c

$(OBJ_DIR)/hello.obj:	$(APP_DIR)/hello/hello.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/hello.obj $(APP_DIR)/hello/hello.c

$(OBJ_DIR)/sample.obj:	$(APP_DIR)/sample/sample.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/sample.obj $(APP_DIR)/sample/sample.c

$(OBJ_DIR)/udp-sample.obj:	$(APP_DIR)/network/udp.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/udp-sample.obj $(APP_DIR)/network/udp.c

$(OBJ_DIR)/tcp-client.obj:	$(APP_DIR)/network/tcp-client.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/tcp-client.obj $(APP_DIR)/network/tcp-client.c

$(OBJ_DIR)/tcp-srv.obj:	$(APP_DIR)/network/tcp-srv.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/tcp-srv.obj $(APP_DIR)/network/tcp-srv.c

$(OBJ_DIR)/tcp-srv-mt.obj:	$(APP_DIR)/network/tcp-srv-mt.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/tcp-srv-mt.obj $(APP_DIR)/network/tcp-srv-mt.c

$(OBJ_DIR)/telnet-srv.obj:	$(APP_DIR)/network/telnet-srv.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/telnet-srv.obj $(APP_DIR)/network/telnet-srv.c

$(OBJ_DIR)/http-srv.obj:	$(APP_DIR)/network/http-srv.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/http-srv.obj $(APP_DIR)/network/http-srv.c

$(OBJ_DIR)/socktests.obj:	$(APP_DIR)/network/socktests.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/socktests.obj $(APP_DIR)/network/socktests.c

$(OBJ_DIR)/monitor.obj:	$(APP_DIR)/monitor/monitor.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/monitor.obj $(APP_DIR)/monitor/monitor.c

$(OBJ_DIR)/term-mon.obj:	$(APP_DIR)/monitor/term-mon.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/term-mon.obj $(APP_DIR)/monitor/term-mon.c

$(OBJ_DIR)/uart-mon.obj:	$(APP_DIR)/monitor/uart-mon.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/uart-mon.obj $(APP_DIR)/monitor/uart-mon.c

$(OBJ_DIR)/telnet-mon.obj:	$(APP_DIR)/monitor/telnet-mon.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/telnet-mon.obj $(APP_DIR)/monitor/telnet-mon.c

$(OBJ_DIR)/uart.obj:	$(APP_DIR)/uart/uart.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/uart.obj $(APP_DIR)/uart/uart.c

$(OBJ_DIR)/uart-sample.obj:	$(APP_DIR)/uart-sample/uart-sample.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/uart-sample.obj $(APP_DIR)/uart-sample/uart-sample.c

$(OBJ_DIR)/video-sample.obj:	$(APP_DIR)/video/video-sample.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/video-sample.obj $(APP_DIR)/video/video-sample.c

$(OBJ_DIR)/exec-sample.obj:	$(APP_DIR)/exec/exec-sample.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/exec-sample.obj $(APP_DIR)/exec/exec-sample.c



########################################################################
#	Base and image related targets
########################################################################

ifeq ($(ARCH), x86)
$(BOOTSEC_DIR)/bootsec.bin: $(BOOTSEC_DIR)/bootsec.asm $(INC_DIR)/bootsec.inc
	$(ASM) -f bin -o $(BOOTSEC_DIR)/bootsec.bin $(BOOTSEC_DIR)/bootsec.asm

$(IMAGE):	$(APP_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(LIBC_OBJS) $(NET_OBJS) $(FS_OBJS)
	$(LD) $(L_OPT) -o $(IMAGE) $(APP_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(LIBC_OBJS) $(NET_OBJS) $(FS_OBJS) # $(GCC_LIB_DIR)/libgcc.a
	$(OBJCOPY) -O pe-i386 --set-section-flags .bss=alloc,contents,load,data --remove-section=.idata $(IMAGE)
endif
ifeq ($(ARCH), arm)
$(IMAGE):	$(OBJ_DIR)/init.obj $(APP_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(LIBC_OBJS) $(NET_OBJS) $(FS_OBJS)
	$(LD) $(L_OPT) -o $(IMAGE) $(APP_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(LIBC_OBJS) $(NET_OBJS) $(FS_OBJS) $(GCC_LIB_DIR)/libgcc.a
	$(OBJCOPY) --set-section-flags .bss=alloc,contents,load,data $(IMAGE)
endif
ifeq ($(ARCH), mips)
$(IMAGE):	$(OBJ_DIR)/init.obj $(APP_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(LIBC_OBJS) $(NET_OBJS) $(FS_OBJS)
	$(LD) $(L_OPT) -o $(IMAGE) $(APP_OBJS) $(SEPTOS_OBJS) $(DRV_OBJS) $(LIBC_OBJS) $(NET_OBJS) $(FS_OBJS) $(GCC_LIB_DIR)/libgcc.a
endif


########################################################################
#	Core objects
########################################################################
$(OBJ_DIR)/sos.lib: $(SEPTOS_OBJS)
	@echo Creating $(OBJ_DIR)/sos.lib
	@lib /nologo /out:$(OBJ_DIR)/sos.lib $(SEPTOS_OBJS)

ifeq ($(ARCH), x86)
$(OBJ_DIR)/init.obj:	$(ARCH_DIR)/init.asm
	$(ASM) $(A_OPT) -o $(OBJ_DIR)/init.obj -f win32 $(ARCH_DIR)/init.asm
endif
ifeq ($(ARCH), arm)
$(OBJ_DIR)/init.obj:	$(ARCH_DIR)/init.s
# Let gcc front-end handle C compiler's "-mabi=apcs-gnu" to assembler's "-meabi=N" translation
	$(CC) $(C_OPT) $(ARCH_DIR)/init.s -o $(OBJ_DIR)/init.obj
endif
ifeq ($(ARCH), mips)
$(OBJ_DIR)/init.obj:	$(ARCH_DIR)/init.s
	$(AS) $(ARCH_DIR)/init.s -o $(OBJ_DIR)/init.obj
endif

$(OBJ_DIR)/sosbasic.obj:	$(SOSBASE_DIR)/sosbasic.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/sosbasic.obj $(SOSBASE_DIR)/sosbasic.c

$(OBJ_DIR)/sosbasic-arch.obj:	$(ARCH_DIR)/sosbasic-arch.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/sosbasic-arch.obj $(ARCH_DIR)/sosbasic-arch.c

$(OBJ_DIR)/memman.obj:	$(SOSBASE_DIR)/memman.c $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/memman.obj $(SOSBASE_DIR)/memman.c

$(OBJ_DIR)/taskman.obj:	$(SOSBASE_DIR)/taskman.c $(INC_DIR)/sosdef.h $(INC_DIR)/taskman.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/taskman.obj $(SOSBASE_DIR)/taskman.c

$(OBJ_DIR)/tasks-arch.obj: $(ARCH_DIR)/tasks-arch.c $(INC_DIR)/sosdef.h $(INC_DIR)/taskman.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/tasks-arch.obj $(ARCH_DIR)/tasks-arch.c

$(OBJ_DIR)/devman.obj: $(SOSBASE_DIR)/devman.c $(INC_DIR)/config.h $(INC_DIR)/sosdef.h $(INC_DIR)/drvint.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/devman.obj $(SOSBASE_DIR)/devman.c

$(OBJ_DIR)/timers.obj: $(SOSBASE_DIR)/timers.c $(INC_DIR)/config.h $(INC_DIR)/sosdef.h $(INC_DIR)/timers.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/timers.obj $(SOSBASE_DIR)/timers.c

$(OBJ_DIR)/timers-arch.obj: $(ARCH_DIR)/timers-arch.c $(INC_DIR)/sosdef.h $(INC_DIR)/timers.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/timers-arch.obj $(ARCH_DIR)/timers-arch.c

$(OBJ_DIR)/io.obj: $(POSIX_DIR)/io.c $(INC_DIR)/config.h $(INC_DIR)/sosdef.h $(INC_DIR)/io.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/io.obj $(POSIX_DIR)/io.c

$(OBJ_DIR)/pthread.obj: $(POSIX_DIR)/pthread.c $(INC_DIR)/config.h $(INC_DIR)/sosdef.h $(INC_DIR)/pthread.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/pthread.obj $(POSIX_DIR)/pthread.c

$(OBJ_DIR)/exec.obj: $(POSIX_DIR)/exec.c $(INC_DIR)/config.h $(INC_DIR)/sosdef.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/exec.obj $(POSIX_DIR)/exec.c


########################################################################
#	Libc objects
########################################################################
$(OBJ_DIR)/rt.lib: $(LIBC_OBJS)
	@echo Creating $(OBJ_DIR)/rt.lib
	@lib /nologo /out:$(OBJ_DIR)/rt.lib $(LIBC_OBJS)

$(OBJ_DIR)/doprnt.obj: $(LIBC_DIR)/doprnt.c
	$(CC) $(C_OPT) -o $(OBJ_DIR)/doprnt.obj $(LIBC_DIR)/doprnt.c

$(OBJ_DIR)/libc.obj: $(LIBC_DIR)/libc.c
	$(CC) $(C_OPT) -o $(OBJ_DIR)/libc.obj $(LIBC_DIR)/libc.c


########################################################################
#	Drivers objects
########################################################################
$(OBJ_DIR)/keyboard.obj: $(DRV_DIR)/keyboard/keyboard.c $(INC_DIR)/keyboard.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/keyboard.obj $(DRV_DIR)/keyboard/keyboard.c

$(OBJ_DIR)/terminal.obj: $(DRV_DIR)/terminal/terminal.c $(INC_DIR)/terminal.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/terminal.obj $(DRV_DIR)/terminal/terminal.c

$(OBJ_DIR)/16x50.obj: $(DRV_DIR)/serial/16x50.c $(INC_DIR)/16x50.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/16x50.obj $(DRV_DIR)/serial/16x50.c

$(OBJ_DIR)/pl011.obj: $(DRV_DIR)/serial/pl011.c $(INC_DIR)/pl011.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/pl011.obj $(DRV_DIR)/serial/pl011.c

$(OBJ_DIR)/pcihost.obj: $(DRV_DIR)/bus/pci/pcihost.c $(INC_DIR)/pcihost.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/pcihost.obj $(DRV_DIR)/bus/pci/pcihost.c

$(OBJ_DIR)/8255x.obj: $(DRV_DIR)/net/8255x.c $(INC_DIR)/8255x.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/8255x.obj $(DRV_DIR)/net/8255x.c
	
$(OBJ_DIR)/am79970.obj: $(DRV_DIR)/net/am79970.c $(INC_DIR)/am79970.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/am79970.obj $(DRV_DIR)/net/am79970.c

$(OBJ_DIR)/smsc91c111.obj: $(DRV_DIR)/net/smsc91c111.c $(INC_DIR)/smsc91c111.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/smsc91c111.obj $(DRV_DIR)/net/smsc91c111.c

$(OBJ_DIR)/dm646x-emac.obj: $(DRV_DIR)/net/dm646x-emac.c $(INC_DIR)/dm646x-emac.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/dm646x-emac.obj $(DRV_DIR)/net/dm646x-emac.c

#$(OBJ_DIR)/dm646x-usb.obj: $(DRV_DIR)/bus/usb/dm646x-usb.c $(INC_DIR)/dm646x-usb.h
#	$(CC) $(C_OPT) -o $(OBJ_DIR)/dm646x-usb.obj $(DRV_DIR)/bus/usb/dm646x-usb.c

$(OBJ_DIR)/dm646x-i2c.obj: $(DRV_DIR)/bus/i2c/dm646x-i2c.c $(INC_DIR)/dm646x-i2c.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/dm646x-i2c.obj $(DRV_DIR)/bus/i2c/dm646x-i2c.c

$(OBJ_DIR)/ehci.obj: $(DRV_DIR)/bus/usb/ehci.c $(INC_DIR)/ehci.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/ehci.obj $(DRV_DIR)/bus/usb/ehci.c

$(OBJ_DIR)/uhci.obj: $(DRV_DIR)/bus/usb/uhci.c $(INC_DIR)/uhci.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/uhci.obj $(DRV_DIR)/bus/usb/uhci.c
	
$(OBJ_DIR)/ide.obj: $(DRV_DIR)/bus/ide/ide.c $(INC_DIR)/ide.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/ide.obj $(DRV_DIR)/bus/ide/ide.c

$(OBJ_DIR)/dm646x-vpif.obj: $(DRV_DIR)/video/dm646x-vpif.c $(INC_DIR)/dm646x-vpif.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/dm646x-vpif.obj $(DRV_DIR)/video/dm646x-vpif.c

$(OBJ_DIR)/tvp7002.obj: $(DRV_DIR)/video/tvp7002.c $(INC_DIR)/tvp7002.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/tvp7002.obj $(DRV_DIR)/video/tvp7002.c


########################################################################
#	Network objetcs
########################################################################

$(OBJ_DIR)/socket.obj: $(NET_DIR)/socket.c $(INC_DIR)/socket.h $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/socket.obj $(NET_DIR)/socket.c

$(OBJ_DIR)/arp.obj: $(NET_DIR)/arp.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/arp.obj $(NET_DIR)/arp.c

$(OBJ_DIR)/ip.obj: $(NET_DIR)/ip.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/ip.obj $(NET_DIR)/ip.c

$(OBJ_DIR)/tcp.obj: $(NET_DIR)/tcp.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/tcp.obj $(NET_DIR)/tcp.c

$(OBJ_DIR)/udp.obj: $(NET_DIR)/udp.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/udp.obj $(NET_DIR)/udp.c

$(OBJ_DIR)/icmp.obj: $(NET_DIR)/icmp.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/icmp.obj $(NET_DIR)/icmp.c

$(OBJ_DIR)/ethernet.obj: $(NET_DIR)/ethernet.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/ethernet.obj $(NET_DIR)/ethernet.c

$(OBJ_DIR)/dhcp.obj: $(NET_DIR)/dhcp.c $(INC_DIR)/inet.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/dhcp.obj $(NET_DIR)/dhcp.c
	

########################################################################
#	Filesystem objects
########################################################################

$(OBJ_DIR)/ext2.obj: $(FS_DIR)/ext2.c $(INC_DIR)/io.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/ext2.obj $(FS_DIR)/ext2.c

$(OBJ_DIR)/fat.obj: $(FS_DIR)/fat.c $(INC_DIR)/io.h
	$(CC) $(C_OPT) -o $(OBJ_DIR)/fat.obj $(FS_DIR)/fat.c

	
