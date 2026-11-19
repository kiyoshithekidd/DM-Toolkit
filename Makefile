#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

APP_TITLE       := DM's Toolkit
APP_DESCRIPTION := Tabletop DM Screen & Fight Tracker
APP_AUTHOR      := KiyoshiTheKidd
ICON            := icon.png

TARGET          := DM_Toolkit
BUILD           := build
SOURCES         := source
DATA            := data
INCLUDES        := include
ROMFS           := romfs

# Host-only targets (do not require DEVKITPRO)
ifeq ($(filter-out test srd,$(MAKECMDGOALS)),)
ifneq ($(MAKECMDGOALS),)
HOST_ONLY := 1
endif
endif

ifdef HOST_ONLY

.PHONY: test srd

srd:
	@python3 tools/compile_srd.py

test: srd
	@g++ -std=c++17 -Wall -Wextra -O2 -Iinclude \
		tests/test_core.cpp source/srd_db.cpp source/combat_tracker.cpp \
		source/campaign_io.cpp source/generators.cpp source/dice_roller.cpp \
		-o test_runner
	@./test_runner

else

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/devkitARM/3ds_rules

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -O2 -mword-relocations -fomit-frame-pointer -ffunction-sections $(ARCH)
CFLAGS   += $(INCLUDE) -D__3DS__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lcitro2d -lcitro3d -lctru -lm

ifeq ($(strip $(CTRU)),)
export CTRU := $(DEVKITPRO)/libctru
endif

LIBDIRS  := $(PORTLIBS) $(CTRU)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
	export LD := $(CC)
else
	export LD := $(CXX)
endif

export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)
export APP_ICON  := $(TOPDIR)/$(ICON)

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean generate cia

all: $(BUILD) $(DEPSDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

generate: $(BUILD)
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i "$(ICON)" -o "$(BUILD)/icon.bin"
	@bannertool makebanner -i "gfx/banner.png" -a "gfx/banner.wav" -o "$(BUILD)/banner.bnr"

cia: all generate
	@$(DEVKITARM)/bin/arm-none-eabi-strip -o $(BUILD)/$(TARGET)-strip.elf $(TARGET).elf
	@makerom -f cia -o $(TARGET).cia -elf $(BUILD)/$(TARGET)-strip.elf -rsf app.rsf -icon $(BUILD)/icon.bin -banner $(BUILD)/banner.bnr -exefslogo -target t

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).cia $(OUTPUT).smdh $(TARGET).elf test_runner

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)

$(OUTPUT).elf  : $(OFILES)

-include $(DEPENDS)

endif
endif
