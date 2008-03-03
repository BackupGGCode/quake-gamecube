# About the release.
VERSION	:= WIP

# Directories.
SRC_DIR		:= ../../src
OBJ_DIR		:= ../../obj/gamecube
DIST_DIR	:= ../../dist/gamecube

# Key files.
DOL				:= $(DIST_DIR)/Quake.dol
ELF				:= $(OBJ_DIR)/Quake.elf
ZIP_FILE		:= Quake_$(VERSION).zip
ZIP_CONTENTS	:= Quake.dol ID1/CONFIG.CFG GPL.txt readme.html

# Compiler flags.
# Note that we MUST USE -fno-merge-constants, because the compiler generates crap otherwise.
COMMON_FLAGS	:= -mcpu=750 -meabi -mhard-float -g -Wall -pipe -mogc
OPT_FLAGS		:= -Os -fno-merge-constants -fno-strict-aliasing -fsingle-precision-constant
DEFINES			:= -DGEKKO
CFLAGS			:= $(COMMON_FLAGS) $(OPT_FLAGS) $(DEFINES) -I$(DEVKITPRO)/libogc/include
CXXFLAGS		:= $(CFLAGS) -Weffc++ -fno-exceptions -fno-rtti
LDFLAGS			:= $(COMMON_FLAGS) -L$(DEVKITPRO)/libogc/lib/cube

# Linker info.
LIBS := -logc -lsdcard

# Files to compile.
OBJS := \
	$(OBJ_DIR)/gamecube/input.o \
	$(OBJ_DIR)/gamecube/main.o \
	$(OBJ_DIR)/gamecube/sound.o \
	$(OBJ_DIR)/gamecube/system.o \
	$(OBJ_DIR)/gamecube/video_software.o \
	$(OBJ_DIR)/gamecube/fileio/sdfileio.o \
	$(OBJ_DIR)/gamecube/tffs/diskio.o \
	$(OBJ_DIR)/gamecube/tffs/tff.o \
	$(OBJ_DIR)/generic/cd_null.o \
	$(OBJ_DIR)/generic/chase.o \
	$(OBJ_DIR)/generic/cl_demo.o \
	$(OBJ_DIR)/generic/cl_input.o \
	$(OBJ_DIR)/generic/cl_main.o \
	$(OBJ_DIR)/generic/cl_parse.o \
	$(OBJ_DIR)/generic/cl_tent.o \
	$(OBJ_DIR)/generic/cmd.o \
	$(OBJ_DIR)/generic/common.o \
	$(OBJ_DIR)/generic/console.o \
	$(OBJ_DIR)/generic/crc.o \
	$(OBJ_DIR)/generic/cvar.o \
	$(OBJ_DIR)/generic/d_edge.o \
	$(OBJ_DIR)/generic/d_fill.o \
	$(OBJ_DIR)/generic/d_init.o \
	$(OBJ_DIR)/generic/d_modech.o \
	$(OBJ_DIR)/generic/d_part.o \
	$(OBJ_DIR)/generic/d_polyse.o \
	$(OBJ_DIR)/generic/d_scan.o \
	$(OBJ_DIR)/generic/d_sky.o \
	$(OBJ_DIR)/generic/d_sprite.o \
	$(OBJ_DIR)/generic/d_surf.o \
	$(OBJ_DIR)/generic/d_vars.o \
	$(OBJ_DIR)/generic/d_zpoint.o \
	$(OBJ_DIR)/generic/draw.o \
	$(OBJ_DIR)/generic/file.o \
	$(OBJ_DIR)/generic/host.o \
	$(OBJ_DIR)/generic/host_cmd.o \
	$(OBJ_DIR)/generic/keys.o \
	$(OBJ_DIR)/generic/mathlib.o \
	$(OBJ_DIR)/generic/menu.o \
	$(OBJ_DIR)/generic/model.o \
	$(OBJ_DIR)/generic/net_loop.o \
	$(OBJ_DIR)/generic/net_main.o \
	$(OBJ_DIR)/generic/net_none.o \
	$(OBJ_DIR)/generic/net_vcr.o \
	$(OBJ_DIR)/generic/pr_cmds.o \
	$(OBJ_DIR)/generic/pr_edict.o \
	$(OBJ_DIR)/generic/pr_exec.o \
	$(OBJ_DIR)/generic/r_aclip.o \
	$(OBJ_DIR)/generic/r_alias.o \
	$(OBJ_DIR)/generic/r_bsp.o \
	$(OBJ_DIR)/generic/r_draw.o \
	$(OBJ_DIR)/generic/r_edge.o \
	$(OBJ_DIR)/generic/r_efrag.o \
	$(OBJ_DIR)/generic/r_light.o \
	$(OBJ_DIR)/generic/r_main.o \
	$(OBJ_DIR)/generic/r_misc.o \
	$(OBJ_DIR)/generic/r_part.o \
	$(OBJ_DIR)/generic/r_sky.o \
	$(OBJ_DIR)/generic/r_sprite.o \
	$(OBJ_DIR)/generic/r_surf.o \
	$(OBJ_DIR)/generic/r_vars.o \
	$(OBJ_DIR)/generic/sbar.o \
	$(OBJ_DIR)/generic/screen.o \
	$(OBJ_DIR)/generic/snd_dma.o \
	$(OBJ_DIR)/generic/snd_mem.o \
	$(OBJ_DIR)/generic/snd_mix.o \
	$(OBJ_DIR)/generic/sv_main.o \
	$(OBJ_DIR)/generic/sv_move.o \
	$(OBJ_DIR)/generic/sv_phys.o \
	$(OBJ_DIR)/generic/sv_user.o \
	$(OBJ_DIR)/generic/view.o \
	$(OBJ_DIR)/generic/wad.o \
	$(OBJ_DIR)/generic/world.o \
	$(OBJ_DIR)/generic/zone.o

# How to build everything.
all: $(DOL)

# How to install.
install: all
	@echo Installing
	@cp $(DOL) S:/
	@cp $(DIST_DIR)/ID1/CONFIG.CFG S:/ID1/

# How to clean.
clean:
	@echo Cleaning
	@rm -f $(OBJS) $(DOL) $(ELF)

# How to build the distribution.
dist: clean all
	@echo Building distribution
	rm -f $(DIST_DIR)/$(ZIP_FILE)
	cd $(DIST_DIR) && zip -9 -r $(ZIP_FILE) $(ZIP_CONTENTS)

# How to make the .DOL file.
$(DOL): $(ELF)
	@echo Creating $(notdir $@)
	@mkdir -p $(dir $@)
	@powerpc-gekko-objcopy -O binary $< $@

# How to link the ELF.
$(ELF): $(OBJS)
	@echo Linking $(notdir $@)
	@mkdir -p $(dir $@)
	@powerpc-gekko-g++ $(LDFLAGS) $(OBJS) $(LIBS) -o $@

# How to compile a C++ source file.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo Compiling $(notdir $<)
	@mkdir -p $(dir $@)
	@powerpc-gekko-g++ $(CXXFLAGS) -c $< -o $@

# How to compile a C source file.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo Compiling $(notdir $<)
	@mkdir -p $(dir $@)
	@powerpc-gekko-gcc $(CFLAGS) -c $< -o $@
