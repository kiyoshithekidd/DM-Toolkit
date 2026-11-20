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
GRAPHICS        := gfx
ROMFS           := romfs
GFXBUILD        := $(ROMFS)/gfx

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
		source/sound_fx.cpp -o test_runner
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
                  $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES  := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
GFXFILES  := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))

ifeq ($(strip $(CPPFILES)),)
	export LD := $(CC)
else
	export LD := $(CXX)
endif

export ROMFS_T3XFILES := $(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES      := $(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))

export OFILES   := $(PICAFILES:.v.pica=.shbin.o) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
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

all: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

generate: $(BUILD)
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -f "visible,allow3d,recordusage,extendedbanner" -i "$(ICON)" -o "$(BUILD)/icon.bin"
	@bannertool makebanner -ci "gfx/banner.cgfx" -a "gfx/banner.wav" -o "$(BUILD)/banner.bnr"

cia: all generate
	@$(DEVKITARM)/bin/arm-none-eabi-strip -o $(BUILD)/$(TARGET)-strip.elf $(TARGET).elf
	@makerom -f cia -o $(TARGET).cia -elf $(BUILD)/$(TARGET)-strip.elf -rsf app.rsf -icon $(BUILD)/icon.bin -banner $(BUILD)/banner.bnr -exefslogo -target t

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).cia $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD) test_runner

$(GFXBUILD)/%.t3x $(BUILD)/%.h : %.t3s
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)

$(OUTPUT).elf  : $(OFILES)

dice_view.o : dice_vsh.shbin.o

%.shbin.o : %.v.pica
	@echo $(notdir $<)
	@picasso -o $*.shbin $<
	@bin2s -a 4 $*.shbin | $(AS) -o $@
	@printf '#pragma once\n#include <3ds/types.h>\nextern const u8 %s_shbin[];\nextern const u32 %s_shbin_size;\n' $* $* > $*_shbin.h

-include $(DEPENDS)

endif
endif
