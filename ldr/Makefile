EXE_NAME=sh_gw_ldr
PRODUCT = sh_gw
VER_MAJ = 1
VER_MIN = 0
VER_PATCH = 0
MAKE_BINARY=yes

TCHAIN = arm-none-eabi-
MCPU += -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb
CDIALECT = gnu99
OPT_LVL = 2
DBG_OPTS = -gdwarf-2 -ggdb -g

CFLAGS   += -fdata-sections -ffunction-sections 
CFLAGS   += -fsingle-precision-constant
CFLAGS   += -fmessage-length=0
CFLAGS   += -fno-exceptions -funsafe-math-optimizations
CFLAGS   += -fno-move-loop-invariants -ffreestanding
CFLAGS   += -Wno-pointer-sign -Wswitch-default -Wshadow -Wno-unused
CFLAGS   += -Wall -Wstrict-prototypes -Wdisabled-optimization -Wformat=2 -Winit-self  -Wmissing-include-dirs
CFLAGS   += -Wstrict-overflow=2
CFLAGS   += -Werror

EXT_LIBS +=c nosys

LDFLAGS  += -specs=nano.specs
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,--print-memory-usage

PPDEFS += USE_STDPERIPH_DRIVER STM32F40_41xxx FW_HEADER_REDUCED PROD_NAME=\"$(PROD_NAME)\" FW_TYPE=FW_LDR DEV=\"GWLDR\" CFG_SYSTEM_SAVES_NON_NATIVE_DATA=1

INCDIR += .
INCDIR += ../common
INCDIR += ../common/adc
INCDIR += ../common/config_system
INCDIR += ../common/CMSIS
INCDIR += ../common/crc
INCDIR += ../common/eth
INCDIR += ../common/eth_con
INCDIR += ../common/fw_header
INCDIR += ../common/i2c
INCDIR += ../common/lwip
INCDIR += ../common/lwip/port/STM32F4x7
INCDIR += ../common/lwip/src/include
INCDIR += ../common/lwip/src/include/ipv4
# INCDIR += ../common/lwip/src/include/ipv6
INCDIR += ../common/spi
INCDIR += ../common/STM32F4x7_ETH_Driver/inc
INCDIR += ../common/STM32F4xx_StdPeriph_Driver/inc

SOURCES += $(call rwildcard, ../common/adc, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/CMSIS, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/config_system, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/crc, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/eth, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/eth_con, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/i2c, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/fw_header, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/spi, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/STM32F4x7_ETH_Driver, *.c *.S *.s)
SOURCES += $(call rwildcard, ../common/STM32F4xx_StdPeriph_Driver, *.c *.S *.s)
SOURCES += $(wildcard ../common/*.c)

SOURCES += $(wildcard ../common/lwip/port/*.c)
SOURCES += $(wildcard ../common/lwip/port/STM32F4x7/*.c)
SOURCES += $(wildcard ../common/lwip/src/api/*.c)
SOURCES += $(wildcard ../common/lwip/src/core/*.c)
SOURCES += $(wildcard ../common/lwip/src/core/ipv4/*.c)
SOURCES += $(wildcard ../common/lwip/src/core/snmp/*.c)
SOURCES += $(wildcard ../common/lwip/src/core/netif/*.c)
SOURCES += $(wildcard ../common/lwip/src/core/netif/ppp/*.c)
SOURCES += $(wildcard ../common/lwip/src/netif/*.c)

SOURCES += $(wildcard *.c)
LDSCRIPT += ../ldscript/ldr.ld

SIGNED_BINARY = $(EXE_NAME)_signed.bin

ARTEFACTS += $(SIGNED_BINARY)

include ../common/core.mk

signer:
	@make -C ../signer_fw_tool -j

# @../signer_fw_tool/signer_fw_tool* $(BINARY) $(BUILDDIR)/$(SIGNED_BINARY) 32768 
$(SIGNED_BINARY): $(BINARY) signer
	@../signer_fw_tool/signer_fw_tool* $(BINARY) $(BUILDDIR)/$(SIGNED_BINARY) 49152 \
		prod=$(PRODUCT) \
		prod_name=$(EXE_NAME) \
		ver_maj=$(VER_MAJ) \
		ver_min=$(VER_MIN) \
		ver_pat=$(VER_PATCH) \
		build_ts=$$(date -u +'%Y%m%d_%H%M%S')

flash: $(SIGNED_BINARY)
	@openocd -f ../target/stm32f4xx.cfg -c "program $(BUILDDIR)/$(SIGNED_BINARY) 0x08004000 verify reset exit" 

debug:
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target remote | openocd -c \"gdb_port pipe\" -f ../target/stm32f4xx.cfg" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit

define tftp_flash
	@atftp --verbose -p -r sh_gw_ldr -l $(2) 192.168.0.$(1)
endef

101: $(SIGNED_BINARY)
	$(call tftp_flash,$@,$(BUILDDIR)/$(SIGNED_BINARY))
102: $(SIGNED_BINARY)
	$(call tftp_flash,$@,$(BUILDDIR)/$(SIGNED_BINARY))
103: $(SIGNED_BINARY)
	$(call tftp_flash,$@,$(BUILDDIR)/$(SIGNED_BINARY))
200: $(SIGNED_BINARY)
	$(call tftp_flash,$@,$(BUILDDIR)/$(SIGNED_BINARY))