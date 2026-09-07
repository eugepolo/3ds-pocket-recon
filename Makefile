.SUFFIXES:
.DEFAULT_GOAL := all
ifeq ($(strip $(DEVKITARM)),)
$(error Set DEVKITPRO and DEVKITARM to your devkitPro installation; see README.md)
endif
include $(DEVKITARM)/3ds_rules

TARGET := pocket-recon
BUILD := build
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -O2 -Wall -Wextra -std=gnu11 -mword-relocations -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__
LDFLAGS = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lctru -lm
export APP_TITLE := Pocket Recon
export APP_DESCRIPTION := Pocket network explorer
export APP_AUTHOR := Pocket Recon contributors

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(CURDIR)/source
export DEPSDIR := $(CURDIR)/$(BUILD)
export INCLUDE := -I$(CURDIR)/include -I$(CTRULIB)/include
export LIBPATHS := -L$(CTRULIB)/lib
export OFILES := main.o scanner.o
export LD := $(CC)
export _3DSXFLAGS := --smdh=$(OUTPUT).smdh
.PHONY: all clean
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	mkdir -p $@
clean:
	rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf
else
all: $(OUTPUT).3dsx
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
