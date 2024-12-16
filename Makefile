EXE_NAME=sh_gw
VER_MAJ =   1
VER_MIN =   0
VER_PATCH = 1
MAKE_BINARY=yes

TCHAIN = arm-none-eabi-
MCPU += -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb
CDIALECT = gnu99
OPT_LVL = 2
DBG_OPTS = -gdwarf-2 -ggdb -g

CFLAGS   += -fvisibility=hidden -funsafe-math-optimizations -fdata-sections -ffunction-sections -fno-move-loop-invariants
CFLAGS   += -fmessage-length=0 -fno-exceptions -fno-common -fno-builtin -ffreestanding
CFLAGS   += -fsingle-precision-constant
CFLAGS   += $(C_FULL_FLAGS)
CFLAGS   += -Werror

CXXFLAGS += -fvisibility=hidden -funsafe-math-optimizations -fdata-sections -ffunction-sections -fno-move-loop-invariants
CXXFLAGS += -fmessage-length=0 -fno-exceptions -fno-common -fno-builtin -ffreestanding
CXXFLAGS += -fvisibility-inlines-hidden -fuse-cxa-atexit -felide-constructors -fno-rtti
CXXFLAGS += -fsingle-precision-constant
CXXFLAGS += $(CXX_FULL_FLAGS)
CXXFLAGS += -Werror

LDFLAGS  += -specs=nano.specs
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,--print-memory-usage

EXT_LIBS +=c m nosys

PPDEFS += USE_STDPERIPH_DRIVER STM32F40_41xxx STM32F407xx FW_TYPE=FW_APP DEV=\"GW\" CFG_SYSTEM_SAVES_NON_NATIVE_DATA=1

PPDEFS += CO_USE_GLOBALS
PPDEFS += CO_CONFIG_FIFO="CO_CONFIG_FIFO_ENABLE|CO_CONFIG_FIFO_ASCII_COMMANDS|CO_CONFIG_FIFO_ASCII_DATATYPES"
PPDEFS += CO_CONFIG_EM="CO_CONFIG_EM_PRODUCER|CO_CONFIG_EM_CONSUMER|CO_CONFIG_EM_HISTORY|CO_CONFIG_EM_STATUS_BITS|CO_CONFIG_FLAG_TIMERNEXT"
PPDEFS += CO_CONFIG_GFC=0
PPDEFS += CO_CONFIG_GTW="CO_CONFIG_GTW_ASCII|CO_CONFIG_GTW_ASCII_SDO|CO_CONFIG_GTW_ASCII_NMT|CO_CONFIG_GTW_ASCII_LSS"
PPDEFS += CO_CONFIG_GTWA_COMM_BUF_SIZE=2000
PPDEFS += CO_CONFIG_GTW_BLOCK_DL_LOOP=3
PPDEFS += CO_CONFIG_HB_CONS="CO_CONFIG_HB_CONS_ENABLE|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
#PPDEFS += CO_CONFIG_LEDS=CO_CONFIG_LEDS_ENABLE
PPDEFS += CO_CONFIG_LEDS=0
PPDEFS += CO_CONFIG_LSS="CO_CONFIG_LSS_MASTER|CO_CONFIG_LSS_SLAVE|CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND"
PPDEFS += CO_CONFIG_NMT="CO_CONFIG_NMT_MASTER|CO_CONFIG_FLAG_TIMERNEXT"
PPDEFS += CO_CONFIG_PDO="CO_CONFIG_RPDO_ENABLE|CO_CONFIG_TPDO_ENABLE|CO_CONFIG_RPDO_TIMERS_ENABLE|CO_CONFIG_TPDO_TIMERS_ENABLE|CO_CONFIG_PDO_SYNC_ENABLE|CO_CONFIG_PDO_OD_IO_ACCESS|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_SDO_CLI="CO_CONFIG_SDO_CLI_ENABLE|CO_CONFIG_SDO_CLI_SEGMENTED|CO_CONFIG_SDO_CLI_LOCAL|CO_CONFIG_FLAG_TIMERNEXT"
PPDEFS += CO_CONFIG_SDO_CLI_BUFFER_SIZE=534
PPDEFS += CO_CONFIG_SDO_SRV="CO_CONFIG_SDO_SRV_SEGMENTED|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_SDO_SRV_BUFFER_SIZE=534
PPDEFS += CO_CONFIG_SRDO=0
PPDEFS += CO_CONFIG_SYNC="CO_CONFIG_SYNC_ENABLE|CO_CONFIG_SYNC_PRODUCER|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_TIME="CO_CONFIG_TIME_ENABLE|CO_CONFIG_TIME_PRODUCER|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_TRACE=0

INCDIR += app
INCDIR += common
INCDIR += common/adc
INCDIR += common/CMSIS
INCDIR += common/canopendriver
INCDIR += common/canopennode
INCDIR += common/config_system
INCDIR += common/crc
INCDIR += common/eth
INCDIR += common/eth_con
INCDIR += common/fw_header
INCDIR += common/i2c
INCDIR += common/logger
INCDIR += common/lwip
INCDIR += common/lwip/port/STM32F4x7
INCDIR += common/lwip/src/include
INCDIR += common/lwip/src/include/ipv4
# INCDIR += common/lwip/src/include/ipv6
INCDIR += common/spi
INCDIR += common/STM32F4x7_ETH_Driver/inc
INCDIR += common/STM32F4xx_StdPeriph_Driver/inc

SOURCES += $(call rwildcard, app, *.c *.S *.s)
SOURCES += $(call rwildcard, common/adc, *.c *.S *.s)
SOURCES += $(call rwildcard, common/canopendriver, *.c *.S *.s)
SOURCES += $(call rwildcard, common/canopennode, *.c *.S *.s)
SOURCES += $(call rwildcard, common/CMSIS, *.c *.S *.s)
SOURCES += $(call rwildcard, common/config_system, *.c *.S *.s)
SOURCES += $(call rwildcard, common/crc, *.c *.S *.s)
SOURCES += $(call rwildcard, common/eth, *.c *.S *.s)
SOURCES += $(call rwildcard, common/eth_con, *.c *.S *.s)
SOURCES += $(call rwildcard, common/fw_header, *.c *.S *.s)
SOURCES += $(call rwildcard, common/i2c, *.c *.S *.s)
SOURCES += $(call rwildcard, common/logger, *.c *.S *.s)
SOURCES += $(call rwildcard, common/spi, *.c *.S *.s)
SOURCES += $(call rwildcard, common/STM32F4x7_ETH_Driver, *.c *.S *.s)
SOURCES += $(call rwildcard, common/STM32F4xx_StdPeriph_Driver, *.c *.S *.s)
SOURCES += $(wildcard common/*.c)

SOURCES += $(wildcard common/lwip/port/*.c)
SOURCES += $(wildcard common/lwip/port/STM32F4x7/*.c)
SOURCES += $(wildcard common/lwip/src/api/*.c)
SOURCES += $(wildcard common/lwip/src/core/*.c)
SOURCES += $(wildcard common/lwip/src/core/ipv4/*.c)
SOURCES += $(wildcard common/lwip/src/core/snmp/*.c)
SOURCES += $(wildcard common/lwip/src/core/netif/*.c)
SOURCES += $(wildcard common/lwip/src/core/netif/ppp/*.c)
SOURCES += $(wildcard common/lwip/src/netif/*.c)
LDSCRIPT += ldscript/app.ld

BINARY_SIGNED = $(BUILDDIR)/$(EXE_NAME)_app_signed.bin
BINARY_MERGED = $(BUILDDIR)/$(EXE_NAME)_full.bin
SIGN = $(BUILDDIR)/sign
ARTEFACTS += $(BINARY_SIGNED)

PRELDR_SIGNED = preldr/build/$(EXE_NAME)_preldr_signed.bin
LDR_SIGNED = ldr/build/$(EXE_NAME)_ldr_signed.bin 
EXT_MAKE_TARGETS = $(PRELDR_SIGNED) $(LDR_SIGNED)

include common/core.mk

$(SIGN): sign/sign.c
	@gcc $< -o $@

$(BINARY_SIGNED): $(BINARY) $(SIGN)
	@$(SIGN) $(BINARY) $@ 503808 \
		prod=$(EXE_NAME) \
		prod_name=$(EXE_NAME)_app \
		ver_maj=$(VER_MAJ) \
		ver_min=$(VER_MIN) \
		ver_pat=$(VER_PATCH) \
		build_ts=$$(date -u +'%Y%m%d_%H%M%S')

$(BINARY_MERGED): $(EXT_MAKE_TARGETS) $(BINARY_SIGNED)
	@echo " [Merging binaries] ..." {$@}
	@cp -f $(PRELDR_SIGNED) $@
	@dd if=$(LDR_SIGNED) of=$@ bs=1 oflag=append seek=16384 status=none
	@dd if=$(BINARY_SIGNED) of=$@ bs=1 oflag=append seek=131072 status=none

.PHONY: composite
composite: $(BINARY_MERGED)

.PHONY: clean
clean: clean_ext_targets

.PHONY: $(EXT_MAKE_TARGETS)
$(EXT_MAKE_TARGETS):
	@$(MAKE) -C $(subst build/,,$(dir $@)) --no-print-directory

.PHONY: clean_ext_targets
clean_ext_targets:
	$(foreach var,$(EXT_MAKE_TARGETS),$(MAKE) -C $(subst build/,,$(dir $(var))) clean;)

#####################
### FLASH & DEBUG ###
#####################

flash: $(BINARY_SIGNED)
	@openocd -d0 -f target/stm32f4xx.cfg -c "program $< 0x08020000 verify reset exit" 

flash_all: $(BINARY_MERGED)
	@openocd -d0 -f target/stm32f4xx.cfg -c "program $< 0x08000000 verify reset exit"

ds:
	@openocd -d0 -f target/stm32f4xx.cfg

debug:
	@set _NO_DEBUG_HEAP=1
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target extended-remote :3333" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit

define tftp_flash
	@echo reset | nc -u 7.7.7.$(1) 2000 -q 0
	@sleep 2.0
	@echo help | nc -u 7.7.7.$(1) 2000 -q 0
	@atftp --verbose -p -r sh_gw_app -l $(2) 7.7.7.$(1)
	@echo reset | nc -u 7.7.7.$(1) 2000 -q 0
endef

11: $(BINARY_SIGNED)
	$(call tftp_flash,$@,$<)
12: $(BINARY_SIGNED)
	$(call tftp_flash,$@,$<)
200: $(BINARY_SIGNED)
	$(call tftp_flash,$@,$<)