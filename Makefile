#------------------------------------------------------------------------------
# CONFIGURE
# - SDK_PATH     : path to SDK directory
#
# - SD_NAME            : e.g s145
# - SD_VERSION         : SoftDevice version e.g 10.0.1
# - SD_HEX             : to bootloader hex binary
# - SIGNED_FW          : if bootloader will ONLY accept signed firmware
# - SIGNED_FW_QX       : Qx for signed firmware verification
# - SIGNED_FW_QY       : Qy for signed firmware verification
# - DUALBANK_FW        : If bootloader will implement a dual bank feature to allow autorecover from failed
# - DEFAULT_TO_OTA_DFU : if entering DFU, by default enter OTA DFU instead of Serial DFU
#------------------------------------------------------------------------------

PYTHON = python

# local customization
-include Makefile.user

# Board specific
-include src/boards/$(BOARD)/board.mk

SDK_PATH     = lib/sdk/components
SDK11_PATH   = lib/sdk11/components
TCRYPT_PATH  = lib/tinycrypt/lib
NRFX_PATH    = lib/nrfx
SD_PATH      = lib/softdevice/$(SD_FILENAME)

# SD_VERSION can be overwritten by board.mk (nRF54LM20A needs v10.0.1, the
# first S145 release with an nRF54LM20A build).
ifndef SD_VERSION
	SD_VERSION = 9.0.0
endif

# SD_CHIP_FAMILY is set per MCU variant below.
SD_NAME = s145

SD_FILENAME  = $(SD_NAME)_$(SD_CHIP_FAMILY)_$(SD_VERSION)
SD_HEX       = $(SD_PATH)/$(SD_FILENAME)_softdevice.hex

# Detect the operating system
ifeq ($(OS),Windows_NT)
	NULL_DEVICE = NUL
else
	NULL_DEVICE = /dev/null
endif

# MCU variant from board.mk (nrf54l15, nrf54l10, nrf54l05 or nrf54lm20a)
ifndef MCU_SUB_VARIANT
  MCU_SUB_VARIANT = nrf54l15
endif

# linker by MCU variant
ifeq ($(DEBUG), 1)
  LD_FILE = linker/$(MCU_SUB_VARIANT)_debug.ld
else
  LD_FILE = linker/$(MCU_SUB_VARIANT).ld
endif

GIT_VERSION := $(shell git describe --dirty --always --tags)

# compiled file name
OUT_NAME = $(BOARD)_bootloader-$(GIT_VERSION)

# merged file = compiled + sd
MERGED_FILE = $(OUT_NAME)_$(SD_NAME)_$(SD_VERSION)


#------------------------------------------------------------------------------
# Tool Configure
#------------------------------------------------------------------------------

# Toolchain commands
CROSS_COMPILE ?= arm-none-eabi-
CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE    = $(CROSS_COMPILE)size
GDB     = $(CROSS_COMPILE)gdb

# Set make directory command
ifneq ($(OS), Windows_NT)
  MKDIR = mkdir -p
else
  MKDIR = mkdir
endif

RM = rm -rf
CP = cp

# Flasher utility options
NRFUTIL = nrfutil
NRFJPROG = nrfjprog
FLASHER ?= nrfjprog

# Flasher will default to nrfjprog
ifeq ($(FLASHER),nrfjprog)
  FLASH_CMD = $(NRFJPROG) --program $1 --sectoranduicrerase -f nrf54l --reset
  FLASH_NOUICR_CMD = $(NRFJPROG) --program $1 -f nrf54l --sectorerase --reset
  FLASH_ERASE_CMD = $(NRFJPROG) -f nrf54l --eraseall
else ifeq ($(FLASHER),pyocd)
  PYOCD ?= pyocd
  FLASH_CMD = $(PYOCD) flash -t nrf54l15 $1
  FLASH_NOUICR_CMD = $(PYOCD) flash -t nrf54l15 $1
  FLASH_ERASE_CMD = $(PYOCD) erase -t nrf54l15 --chip
else
  $(error Unsupported flash utility: "$(FLASHER)")
endif

# Build directory
BUILD = _build/build-$(BOARD)
BIN = _bin/$(BOARD)

# nRF54L MCU variant selection
#
# SD_CHIP_FAMILY selects the vendored SoftDevice directory (together with
# SD_VERSION); DEBUG_BOOTLOADER_REGION_START must track the FLASH origin of the
# matching linker/$(MCU_SUB_VARIANT)_debug.ld.
#
# Note -DNRF54LM20A_XXAA is passed on its own: nrf.h tests NRF54L15_XXAA before
# NRF54LM20A_XXAA, so pairing them would select the wrong device header. The
# L10/L05 pairing is deliberate and stays as-is.
ifeq ($(MCU_SUB_VARIANT),nrf54l15)
  CFLAGS += -DNRF54L15_XXAA
  DFU_DEV_REV = 54115
  DFU_APP_DATA_RESERVED = 10*4096
  SD_CHIP_FAMILY = nrf54l
  DEBUG_BOOTLOADER_REGION_START = 0x148000
else ifeq ($(MCU_SUB_VARIANT),nrf54l10)
  CFLAGS += -DNRF54L10_XXAA -DNRF54L15_XXAA
  DFU_DEV_REV = 54110
  DFU_APP_DATA_RESERVED = 2*4096
  SD_CHIP_FAMILY = nrf54l
  DEBUG_BOOTLOADER_REGION_START = 0x0C8000
else ifeq ($(MCU_SUB_VARIANT),nrf54l05)
  CFLAGS += -DNRF54L05_XXAA -DNRF54L15_XXAA
  DFU_DEV_REV = 54105
  DFU_APP_DATA_RESERVED = 2*4096
  SD_CHIP_FAMILY = nrf54l
  DEBUG_BOOTLOADER_REGION_START = 0x048000
else ifeq ($(MCU_SUB_VARIANT),nrf54lm20a)
  CFLAGS += -DNRF54LM20A_XXAA
  DFU_DEV_REV = 54120
  DFU_APP_DATA_RESERVED = 10*4096
  SD_CHIP_FAMILY = nrf54lm20
  DEBUG_BOOTLOADER_REGION_START = 0x1C8000
else
  $(error Unknown MCU_SUB_VARIANT: $(MCU_SUB_VARIANT))
endif


# Startup file: the vector table layout differs between the nRF54L and nRF54LM
# parts, so LM20A must use its own. L10/L05 share the L15 table.
ifeq ($(MCU_SUB_VARIANT),nrf54lm20a)
  MCU_STARTUP_VARIANT = nrf54lm20a
else
  MCU_STARTUP_VARIANT = nrf54l15
endif

SD_NAME_UPPER = $(subst s,S,${SD_NAME})
CFLAGS += -D$(SD_NAME_UPPER)

#----------------------------------
# ANT_LICENSE_KEY handling
#----------------------------------
ifdef ANT_LICENSE_KEY
  CFLAGS += -DANT_LICENSE_KEY=\"$(ANT_LICENSE_KEY)\"
endif

#------------------------------------------------------------------------------
# SOURCE FILES
#------------------------------------------------------------------------------

# all files in src
C_SRC += \
  src/dfu_ble_svc.c \
  src/dfu_init.c \
  src/flash_nrf5x.c \
  src/main.c \
  src/screen.c \
  src/images.c \

# if using a signed firmware
ifeq ($(SIGNED_FW), 1)
C_SRC += \
  $(TCRYPT_PATH)/source/sha256.c \
  $(TCRYPT_PATH)/source/ecc.c \
  $(TCRYPT_PATH)/source/ecc_dsa.c \
  $(TCRYPT_PATH)/source/utils.c
endif

# all files in boards
C_SRC += src/boards/boards.c
C_SRC += src/boards/$(BOARD)/pinconfig.c

# nrfx (4.x — MDK lives under bsp/stable/mdk)
C_SRC += $(NRFX_PATH)/drivers/src/nrfx_power.c
C_SRC += $(NRFX_PATH)/bsp/stable/mdk/system_nrf54l.c

# SDK 11 files: serial + OTA DFU
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/bootloader.c
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/bootloader_settings.c
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/bootloader_util.c
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/dfu_transport_serial.c
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/dfu_transport_ble.c
ifeq ($(DUALBANK_FW), 1)
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/dfu_dual_bank.c
else
C_SRC += $(SDK11_PATH)/libraries/bootloader_dfu/dfu_single_bank.c
endif
C_SRC += $(SDK11_PATH)/ble/ble_services/ble_dfu/ble_dfu.c
C_SRC += $(SDK11_PATH)/ble/ble_services/ble_dis/ble_dis.c
C_SRC += $(SDK11_PATH)/drivers_nrf/pstorage/pstorage_raw.c

# Latest SDK files: peripheral drivers
C_SRC += src/app_timer_nrf54l.c
C_SRC += $(SDK_PATH)/libraries/scheduler/app_scheduler.c
C_SRC += $(SDK_PATH)/libraries/util/app_error.c
C_SRC += $(SDK_PATH)/libraries/util/app_util_platform.c
C_SRC += $(SDK_PATH)/libraries/crc16/crc16.c
C_SRC += $(SDK_PATH)/libraries/hci/hci_mem_pool.c
C_SRC += $(SDK_PATH)/libraries/hci/hci_slip.c
C_SRC += $(SDK_PATH)/libraries/hci/hci_transport.c
C_SRC += $(SDK_PATH)/libraries/util/nrf_assert.c

# UART Serial transport (nRF54L has no USB)
C_SRC += $(SDK_PATH)/libraries/uart/app_uart.c
C_SRC += $(SDK_PATH)/drivers_nrf/uart/nrf_drv_uart.c
C_SRC += $(SDK_PATH)/drivers_nrf/common/nrf_drv_common.c


#------------------------------------------------------------------------------
# Assembly Files
#------------------------------------------------------------------------------
ASM_SRC = $(NRFX_PATH)/bsp/stable/mdk/gcc_startup_$(MCU_STARTUP_VARIANT)_application.S

#------------------------------------------------------------------------------
# INCLUDE PATH
#------------------------------------------------------------------------------

# src
IPATH += \
  src \
  src/boards \
  src/boards/$(BOARD) \
  src/cmsis/include

ifeq ($(SIGNED_FW), 1)
IPATH += \
  $(TCRYPT_PATH)/include
endif

# nrfx 4.x — MDK and soc live under bsp/stable/
IPATH += \
  $(NRFX_PATH) \
  $(NRFX_PATH)/bsp/stable/mdk \
  $(NRFX_PATH)/hal \
  $(NRFX_PATH)/haly \
  $(NRFX_PATH)/drivers/include \
  $(NRFX_PATH)/drivers/src \
  $(NRFX_PATH)/helpers \
  $(NRFX_PATH)/lib \
  $(NRFX_PATH)/bsp/stable \
  $(NRFX_PATH)/bsp/stable/soc \
  $(NRFX_PATH)/bsp/stable/soc/irqs

# sdk11 for ble dfu
IPATH += \
  $(SDK11_PATH)/libraries/bootloader_dfu/hci_transport \
  $(SDK11_PATH)/libraries/bootloader_dfu \
  $(SDK11_PATH)/libraries/util \
  $(SDK11_PATH)/drivers_nrf/pstorage \
  $(SDK11_PATH)/ble/common \
  $(SDK11_PATH)/ble/ble_services/ble_dfu \
  $(SDK11_PATH)/ble/ble_services/ble_dis

# later sdk with updated drivers
IPATH += \
  $(SDK_PATH)/libraries/timer \
  $(SDK_PATH)/libraries/scheduler \
  $(SDK_PATH)/libraries/crc16 \
  $(SDK_PATH)/libraries/util \
  $(SDK_PATH)/libraries/hci/config \
  $(SDK_PATH)/libraries/uart \
  $(SDK_PATH)/libraries/hci \
  $(SDK_PATH)/drivers_nrf/delay \
  $(SDK_PATH)/drivers_nrf/common \
  $(SDK_PATH)/drivers_nrf/uart

# SoftDevice
IPATH += \
  $(SD_PATH)/$(SD_FILENAME)_API/include

#------------------------------------------------------------------------------
# Compiler Flags
#------------------------------------------------------------------------------

#flags common to all targets
CFLAGS += \
	-mthumb \
	-mabi=aapcs \
	-mcpu=cortex-m33 \
	-mfloat-abi=hard \
	-mfpu=fpv5-sp-d16 \
	-ggdb \
	-Os \
	-ffunction-sections \
	-fdata-sections \
	-fno-builtin \
	-fshort-enums \
	-fstack-usage \
	-fno-strict-aliasing \
	-Wall \
	-Wextra \
	-Werror \
	-Wfatal-errors \
	-Werror-implicit-function-declaration \
	-Wfloat-equal \
	-Wundef \
	-Wshadow \
	-Wwrite-strings \
	-Wsign-compare \
	-Wmissing-format-attribute \
	-Wno-endif-labels \
	-Wunreachable-code

# Suppress warning caused by SDK
CFLAGS += -Wno-unused-parameter -Wno-expansion-to-defined -Wno-array-bounds

# Nordic Softdevice SDK header files contains inline assembler that has
# broken constraints. As a result the IPA-modref pass, introduced in gcc-11,
# is able to "prove" that arguments to wrapper functions generated with
# the SVCALL() macro are unused and, as a result, the optimizer will remove
# code within the callers that sets up these arguments (which results in
# a broken bootloader). The broken headers come from Nordic-supplied zip
# files and are not trivial to patch so, for now, we'll simply disable the
# new gcc-11 inter-procedural optimizations.
ifeq (,$(findstring unrecognized,$(shell $(CC) $(CFLAGS) -fno-ipa-modref 2>&1)))
CFLAGS += -fno-ipa-modref
endif

# Defined Symbol (MACROS)
CFLAGS += -D__HEAP_SIZE=0
CFLAGS += -DCONFIG_GPIO_AS_PINRESET

CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -DBLEDIS_FW_VERSION='"$(GIT_VERSION) $(SD_NAME) $(SD_VERSION)"'

ifeq ($(SIGNED_FW), 1)
CFLAGS += -DSIGNED_FW
CFLAGS += -DSIGNED_FW_QX='$(SIGNED_FW_QX)'
CFLAGS += -DSIGNED_FW_QY='$(SIGNED_FW_QY)'
endif

ifeq ($(DEFAULT_TO_OTA_DFU), 1)
CFLAGS += -DDEFAULT_TO_OTA_DFU
endif

_VER = $(subst ., ,$(word 1, $(subst -, ,$(GIT_VERSION))))
CFLAGS += -DMK_BOOTLOADER_VERSION='($(word 1,$(_VER)) << 16) + ($(word 2,$(_VER)) << 8) + $(word 3,$(_VER))'

# Debug option use RTT for printf
ifeq ($(DEBUG), 1)
  CFLAGS += -DCFG_DEBUG -DSEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
  RTT_SRC = lib/SEGGER_RTT
  IPATH += $(RTT_SRC)/RTT
  C_SRC += $(RTT_SRC)/RTT/SEGGER_RTT.c
  DFU_APP_DATA_RESERVED = 0
  CFLAGS += -DBOOTLOADER_REGION_START=$(DEBUG_BOOTLOADER_REGION_START)
endif

CFLAGS += -DDFU_APP_DATA_RESERVED=$(DFU_APP_DATA_RESERVED)

#------------------------------------------------------------------------------
# Linker Flags
#------------------------------------------------------------------------------

LDFLAGS += \
	$(CFLAGS) \
	-Wl,-L,linker -Wl,-T,$(LD_FILE) \
	-Wl,--print-memory-usage \
	-Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections \
	-specs=nosys.specs -specs=nano.specs

LIBS += -lm -lc

#------------------------------------------------------------------------------
# Assembler flags
#------------------------------------------------------------------------------

ASFLAGS += $(CFLAGS)

#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

C_SOURCE_FILE_NAMES = $(notdir $(C_SRC))
C_PATHS = $(call remduplicates, $(dir $(C_SRC) ) )
C_OBJECTS = $(addprefix $(BUILD)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SRC))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SRC) ))
ASM_OBJECTS = $(addprefix $(BUILD)/, $(ASM_SOURCE_FILE_NAMES:.S=.o) )

vpath %.c $(C_PATHS)
vpath %.S $(ASM_PATHS)

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

INC_PATHS = $(addprefix -I,$(IPATH))

#------------------------------------------------------------------------------
# BUILD TARGETS
#------------------------------------------------------------------------------

.PHONY: all clean flash flash-dfu flash-sd erase gdbflash gdb

# default target to build
all: $(BUILD)/$(OUT_NAME).out $(BUILD)/$(OUT_NAME).hex $(BUILD)/$(MERGED_FILE).hex $(BUILD)/$(MERGED_FILE).zip

# Print out the value of a make variable.
print-%:
	@echo $* = $($*)

#------------------- Compile rules -------------------

# Create build directories
$(BUILD):
	@$(MKDIR) "$@"

clean:
	@$(RM) $(BUILD)
	@$(RM) $(BIN)

# linkermap must be install previously at https://github.com/hathach/linkermap
linkermap: $(BUILD)/$(OUT_NAME).out
	@linkermap -v $<.map

# Create objects from C SRC files
$(BUILD)/%.o: %.c
	@echo CC $(notdir $<)
	@$(CC) $(CFLAGS) $(INC_PATHS) -c -o $@ $<

# Assemble files
$(BUILD)/%.o: %.S
	@echo AS $(notdir $<)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) $(INC_PATHS) -c -o $@ $<

# Link
$(BUILD)/$(OUT_NAME).out: $(BUILD) $(OBJECTS)
	@echo LD $(notdir $@)
	@$(CC) -o $@ $(LDFLAGS) $(OBJECTS) -Wl,--start-group $(LIBS) -Wl,--end-group
	@$(SIZE) $@

#------------------- Binary generator -------------------

# Create hex file
$(BUILD)/$(OUT_NAME).hex: $(BUILD)/$(OUT_NAME).out
	@echo Create $(notdir $@)
	@$(OBJCOPY) -O ihex $< $@

# merge bootloader and sd hex together
$(BUILD)/$(MERGED_FILE).hex: $(BUILD)/$(OUT_NAME).hex
	@echo Create $(notdir $@)
	@$(PYTHON) tools/hexmerge.py -o $@ $< $(SD_HEX)

# Create pkg zip file for bootloader+SD combo to use with DFU serial
$(BUILD)/$(MERGED_FILE).zip: $(BUILD)/$(OUT_NAME).hex
	@$(NRFUTIL) dfu genpkg --dev-type 0x0054 --dev-revision $(DFU_DEV_REV) --bootloader $< --softdevice $(SD_HEX) $@

#-------------- Artifacts --------------
$(BIN):
	@$(MKDIR) -p $@

copy-artifact: $(BIN)
	@$(CP) $(BUILD)/$(MERGED_FILE).hex $(BIN)
	@$(CP) $(BUILD)/$(MERGED_FILE).zip $(BIN)

#--------------------------------------
# Flash Target
#--------------------------------------

check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))

# erase chip
erase:
	@echo Erasing flash
	$(call FLASH_ERASE_CMD)

# Flash the compiled
flash: $(BUILD)/$(OUT_NAME).hex
	@echo Flashing: $(notdir $<)
	$(call FLASH_CMD,$<)

# flash SD only
sd: flash-sd
flash-sd:
	@echo Flashing: $(SD_HEX)
	$(call FLASH_NOUICR_CMD,$(SD_HEX))

# dfu with nrfutil using serial interface
dfu-flash: flash-dfu
flash-dfu: $(BUILD)/$(MERGED_FILE).zip
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(NRFUTIL) --verbose dfu serial --package $< -p $(SERIAL) -b 115200 --singlebank --touch 1200

#------------------- Debugging -------------------

gdbflash: $(BUILD)/$(MERGED_FILE).hex
	@echo Flashing: $<
	@$(GDB_BMP) -nx --batch -ex 'load $<' -ex 'compare-sections' -ex 'kill'

gdb: $(BUILD)/$(OUT_NAME).out
	$(GDB_BMP) $<
