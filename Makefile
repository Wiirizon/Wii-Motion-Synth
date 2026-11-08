#---------------------------------------------------------------------------------
# Wii Motion Synth - Makefile
#---------------------------------------------------------------------------------

APP_NAME    := WiiMotionSynth
BUILD_DIR   := build
SRCS        := main.c synth_core.c preset_store.c preset_io.c preset_fs_stdio.c classic_input.c

# Rely on environment variables set by devkitPro instead of hardcoded Windows paths
ifeq ($(strip $(DEVKITPRO)),)
  $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

ifeq ($(strip $(DEVKITPPC)),)
  $(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

PREFIX      := $(DEVKITPPC)/bin/powerpc-eabi-
CC          := $(PREFIX)gcc

# Path to the elf2dol executable using environment variables
ELF2DOL     := $(DEVKITPRO)/tools/bin/elf2dol

INCLUDES    := -I$(DEVKITPRO)/libogc/include \
               -I$(DEVKITPRO)/libogc/include/wiiuse

LIBPATHS    := -L$(DEVKITPRO)/libogc/lib/wii \
               -L$(DEVKITPPC)/powerpc-eabi/lib

CFLAGS      := -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -O2 -Wall $(INCLUDES)
# Included Wiimote (wiiuse) and Bluetooth (bte) libraries
LIBS        := -lwiiuse -lbte -lasnd -lfat -logc -lm
LDFLAGS     := $(LIBPATHS) -mrvl -Wl,-gc-sections $(LIBS)

all: $(BUILD_DIR)/$(APP_NAME).dol

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/$(APP_NAME).elf: $(SRCS) | $(BUILD_DIR)
	@echo "Compiling $(APP_NAME).elf..."
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(APP_NAME).dol: $(BUILD_DIR)/$(APP_NAME).elf
	@echo "Converting to Wii executable (.dol)..."
	$(ELF2DOL) $< $@
	@echo "Success! File generated at $@"

clean:
	@rm -rf $(BUILD_DIR)

.PHONY: all clean