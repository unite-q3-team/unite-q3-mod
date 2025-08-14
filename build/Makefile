#
# Unified Makefile for native (shared libs) and QVM builds
# - One entry point in build/ directory
# - MSVC is not used; MinGW (gcc) supported on Windows
#

# Detect compile host platform/arch
COMPILE_PLATFORM=$(shell uname | sed -e 's/_.*//' | tr '[:upper:]' '[:lower:]' | sed -e 's/\//_/g')
COMPILE_ARCH=$(shell uname -m | sed -e 's/i.86/x86/' | sed -e 's/^arm.*/arm/')

ifeq ($(COMPILE_PLATFORM),mingw32)
  ifeq ($(COMPILE_ARCH),i386)
    COMPILE_ARCH=x86
  endif
endif

#############################################################################
#
# Local overrides
#
#############################################################################
-include Makefile.local

ifndef MOD
MOD=baseq3a
endif

ifeq ($(COMPILE_PLATFORM),cygwin)
  PLATFORM=mingw32
endif

ifndef PLATFORM
PLATFORM=$(COMPILE_PLATFORM)
endif
export PLATFORM

ifeq ($(PLATFORM),mingw32)
  MINGW=1
endif
ifeq ($(PLATFORM),mingw64)
  MINGW=1
endif

ifeq ($(COMPILE_ARCH),i86pc)
  COMPILE_ARCH=x86
endif

ifeq ($(COMPILE_ARCH),amd64)
  COMPILE_ARCH=x86_64
endif
ifeq ($(COMPILE_ARCH),x64)
  COMPILE_ARCH=x86_64
endif

ifndef ARCH
ARCH=$(COMPILE_ARCH)
endif
export ARCH

ifneq ($(PLATFORM),$(COMPILE_PLATFORM))
  CROSS_COMPILING=1
else
  CROSS_COMPILING=0

  ifneq ($(ARCH),$(COMPILE_ARCH))
    CROSS_COMPILING=1
  endif
endif
export CROSS_COMPILING

ifndef BUILD_DIR
BUILD_DIR=build
endif

ifndef TEMPDIR
TEMPDIR=/tmp
endif

ifndef MOUNT_DIR
MOUNT_DIR=../code
endif

ifndef DEBUG_CFLAGS
DEBUG_CFLAGS=-g -O0
endif

BD=$(BUILD_DIR)/debug-$(PLATFORM)-$(ARCH)
BR=$(BUILD_DIR)/release-$(PLATFORM)-$(ARCH)

QADIR=$(MOUNT_DIR)/game
CGDIR=$(MOUNT_DIR)/cgame
UIDIR=$(MOUNT_DIR)/q3_ui

bin_path=$(shell which $(1) 2> /dev/null)

#############################################################################
# SETUP AND BUILD -- native shared libraries (Linux/MinGW)
#############################################################################

## Defaults
LIB=lib

INSTALL=install
MKDIR=mkdir

ifeq ($(PLATFORM),linux)

  ifeq ($(ARCH),x86_64)
    LIB=lib64
  endif

  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes -pipe

  OPTIMIZE = -O2 -fvisibility=hidden -fomit-frame-pointer -ffast-math

  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC -fvisibility=hidden
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  LIBS= -lm

  ifeq ($(ARCH),x86)
    BASE_CFLAGS += -m32
  endif

endif # Linux


ifdef MINGW

  ifeq ($(CROSS_COMPILING),1)
    ifneq ($(findstring $(strip $(CC)),cc gcc),)
      CC=
    endif

    ifeq ($(ARCH),x86_64)
      MINGW_PREFIXES=x86_64-w64-mingw32 amd64-mingw32msvc
      STRIP=x86_64-w64-mingw32-strip
    endif
    ifeq ($(ARCH),x86)
      MINGW_PREFIXES=i686-w64-mingw32 i586-mingw32msvc i686-pc-mingw32
    endif

    ifndef CC
      CC=$(firstword $(strip $(foreach MINGW_PREFIX, $(MINGW_PREFIXES), \
         $(call bin_path, $(MINGW_PREFIX)-gcc))))
    endif

    ifndef WINDRES
      WINDRES=$(firstword $(strip $(foreach MINGW_PREFIX, $(MINGW_PREFIXES), \
         $(call bin_path, $(MINGW_PREFIX)-windres))))
    endif
  else
    ifeq ($(call bin_path, $(CC)),)
      CC=gcc
    endif

  endif

  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes -pipe

  OPTIMIZE = -O2 -fvisibility=hidden -fomit-frame-pointer -ffast-math

  SHLIBEXT=dll
  SHLIBCFLAGS=-fPIC -fvisibility=hidden
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  LIBS= -lm

  ifeq ($(ARCH),x86)
    BASE_CFLAGS += -m32
  endif

endif # MINGW


TARGETS =

ifndef FULLBINEXT
  FULLBINEXT=.$(ARCH)$(BINEXT)
endif

ifndef SHLIBNAME
  SHLIBNAME=$(ARCH).$(SHLIBEXT)
endif

TARGETS += \
  $(B)/$(MOD)/cgame$(SHLIBNAME) \
  $(B)/$(MOD)/qagame$(SHLIBNAME) \
  $(B)/$(MOD)/ui$(SHLIBNAME)

ifeq ("$(CC)", $(findstring "$(CC)", "clang" "clang++"))
  BASE_CFLAGS += -Qunused-arguments
endif

ifdef DEFAULT_BASEDIR
  BASE_CFLAGS += -DDEFAULT_BASEDIR=\"$(DEFAULT_BASEDIR)\"
endif

ifeq ($(NO_STRIP),1)
  STRIP_FLAG =
else
  STRIP_FLAG = -s
endif

BASE_CFLAGS += -Wformat=2 -Wno-format-zero-length -Wformat-security -Wno-format-nonliteral
BASE_CFLAGS += -Wstrict-aliasing=2 -Wmissing-format-attribute
BASE_CFLAGS += -Wdisabled-optimization
BASE_CFLAGS += -Werror-implicit-function-declaration

ifeq ($(V),1)
echo_cmd=@:
Q=
else
echo_cmd=@echo
Q=@
endif

define DO_CC
$(echo_cmd) "CC $<"
$(Q)$(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) $(OPTIMIZE) -o $@ -c $<
endef

define DO_SHLIB_CC
$(echo_cmd) "SHLIB_CC $<"
$(Q)$(CC) $(MOD_CFLAGS) $(SHLIBCFLAGS) $(CFLAGS) $(OPTIMIZE) -o $@ -c $<
endef

define DO_GAME_CC
$(echo_cmd) "GAME_CC $<"
$(Q)$(CC) $(MOD_CFLAGS) -DQAGAME $(SHLIBCFLAGS) $(CFLAGS) $(OPTIMIZE) -o $@ -c $<
endef

define DO_CGAME_CC
$(echo_cmd) "CGAME_CC $<"
$(Q)$(CC) $(MOD_CFLAGS) -DCGAME $(SHLIBCFLAGS) $(CFLAGS) $(OPTIMIZE) -o $@ -c $<
endef

define DO_UI_CC
$(echo_cmd) "UI_CC $<"
$(Q)$(CC) $(MOD_CFLAGS) -DUI $(SHLIBCFLAGS) $(CFLAGS) $(OPTIMIZE) -o $@ -c $<
endef

#############################################################################
# MAIN TARGETS (native)
#############################################################################

default: release
all: debug release

debug:
	@$(MAKE) targets B=$(BD) CFLAGS="$(CFLAGS) $(BASE_CFLAGS)" \
	  OPTIMIZE="$(DEBUG_CFLAGS)" V=$(V)

release:
	@$(MAKE) targets B=$(BR) CFLAGS="$(CFLAGS) $(BASE_CFLAGS)" \
	  OPTIMIZE="-DNDEBUG $(OPTIMIZE)" V=$(V)

# Create the build directories, check libraries and print out
# an informational message, then start building
targets: makedirs
	@echo ""
	@echo "Building $(CLIENTBIN) in $(B):"
	@echo "  PLATFORM: $(PLATFORM)"
	@echo "  ARCH: $(ARCH)"
	@echo "  COMPILE_PLATFORM: $(COMPILE_PLATFORM)"
	@echo "  COMPILE_ARCH: $(COMPILE_ARCH)"
	@echo "  CC: $(CC)"
	@echo ""
	@echo "  CFLAGS:"
	-@for i in $(CFLAGS); \
	do \
		echo "    $$i"; \
	done
	-@for i in $(OPTIMIZE); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  LDFLAGS:"
	-@for i in $(LDFLAGS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  LIBS:"
	-@for i in $(LIBS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  Output:"
	-@for i in $(TARGETS); \
	do \
		echo "    $$i"; \
	done
	@echo ""

ifneq ($(TARGETS),)
	@$(MAKE) $(TARGETS) V=$(V)
endif

makedirs:
	@if [ ! -d $(BUILD_DIR) ];then $(MKDIR) $(BUILD_DIR);fi
	@if [ ! -d $(B) ];then $(MKDIR) $(B);fi
	@if [ ! -d $(B)/$(MOD) ];then $(MKDIR) $(B)/$(MOD);fi
	@if [ ! -d $(B)/$(MOD)/cgame ];then $(MKDIR) $(B)/$(MOD)/cgame;fi
	@if [ ! -d $(B)/$(MOD)/game ];then $(MKDIR) $(B)/$(MOD)/game;fi
	@if [ ! -d $(B)/$(MOD)/ui ];then $(MKDIR) $(B)/$(MOD)/ui;fi
	@if [ ! -d $(B)/$(MOD)/vm ];then $(MKDIR) $(B)/$(MOD)/vm;fi

#############################################################################
## BASEQ3 CGAME (native)
#############################################################################

# $(B)/$(MOD)/cgame/cg_particles.o \

CGOBJ_ = \
  $(B)/$(MOD)/cgame/cg_main.o \
  $(B)/$(MOD)/cgame/bg_lib.o \
  $(B)/$(MOD)/cgame/bg_misc.o \
  $(B)/$(MOD)/cgame/bg_pmove.o \
  $(B)/$(MOD)/cgame/bg_slidemove.o \
  $(B)/$(MOD)/cgame/cg_consolecmds.o \
  $(B)/$(MOD)/cgame/cg_draw.o \
  $(B)/$(MOD)/cgame/cg_drawtools.o \
  $(B)/$(MOD)/cgame/cg_effects.o \
  $(B)/$(MOD)/cgame/cg_ents.o \
  $(B)/$(MOD)/cgame/cg_event.o \
  $(B)/$(MOD)/cgame/cg_info.o \
  $(B)/$(MOD)/cgame/cg_localents.o \
  $(B)/$(MOD)/cgame/cg_marks.o \
  $(B)/$(MOD)/cgame/cg_players.o \
  $(B)/$(MOD)/cgame/cg_playerstate.o \
  $(B)/$(MOD)/cgame/cg_predict.o \
  $(B)/$(MOD)/cgame/cg_scoreboard.o \
  $(B)/$(MOD)/cgame/cg_servercmds.o \
  $(B)/$(MOD)/cgame/cg_snapshot.o \
  $(B)/$(MOD)/cgame/cg_view.o \
  $(B)/$(MOD)/cgame/cg_weapons.o \
  \
  $(B)/$(MOD)/game/q_math.o \
  $(B)/$(MOD)/game/q_shared.o

CGOBJ = $(CGOBJ_) $(B)/$(MOD)/cgame/cg_syscalls.o

$(B)/$(MOD)/cgame$(SHLIBNAME): $(CGOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(CGOBJ)

#############################################################################
## BASEQ3 GAME (native)
#############################################################################

QAOBJ_ = \
  $(B)/$(MOD)/game/g_main.o \
  $(B)/$(MOD)/game/ai_chat.o \
  $(B)/$(MOD)/game/ai_cmd.o \
  $(B)/$(MOD)/game/ai_dmnet.o \
  $(B)/$(MOD)/game/ai_dmq3.o \
  $(B)/$(MOD)/game/ai_main.o \
  $(B)/$(MOD)/game/ai_team.o \
  $(B)/$(MOD)/game/ai_vcmd.o \
  $(B)/$(MOD)/game/bg_lib.o \
  $(B)/$(MOD)/game/bg_misc.o \
  $(B)/$(MOD)/game/bg_pmove.o \
  $(B)/$(MOD)/game/bg_slidemove.o \
  $(B)/$(MOD)/game/g_active.o \
  $(B)/$(MOD)/game/g_arenas.o \
  $(B)/$(MOD)/game/g_bot.o \
  $(B)/$(MOD)/game/g_client.o \
  $(B)/$(MOD)/game/g_cmds.o \
  $(B)/$(MOD)/game/g_combat.o \
  $(B)/$(MOD)/game/g_items.o \
  $(B)/$(MOD)/game/g_mem.o \
  $(B)/$(MOD)/game/g_misc.o \
  $(B)/$(MOD)/game/g_missile.o \
  $(B)/$(MOD)/game/g_mover.o \
  $(B)/$(MOD)/game/g_rotation.o \
  $(B)/$(MOD)/game/g_session.o \
  $(B)/$(MOD)/game/g_spawn.o \
  $(B)/$(MOD)/game/g_svcmds.o \
  $(B)/$(MOD)/game/g_target.o \
  $(B)/$(MOD)/game/g_team.o \
  $(B)/$(MOD)/game/g_trigger.o \
  $(B)/$(MOD)/game/g_utils.o \
  $(B)/$(MOD)/game/g_unlagged.o \
  $(B)/$(MOD)/game/g_weapon.o \
  \
  $(B)/$(MOD)/game/q_math.o \
  $(B)/$(MOD)/game/q_shared.o

QAOBJ = $(QAOBJ_) $(B)/$(MOD)/game/g_syscalls.o

$(B)/$(MOD)/qagame$(SHLIBNAME): $(QAOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(QAOBJ)

#############################################################################
## BASEQ3 UI (native)
#############################################################################

UIOBJ_ = \
  $(B)/$(MOD)/ui/ui_main.o \
  $(B)/$(MOD)/ui/bg_misc.o \
  $(B)/$(MOD)/ui/bg_lib.o \
  $(B)/$(MOD)/ui/ui_addbots.o \
  $(B)/$(MOD)/ui/ui_atoms.o \
  $(B)/$(MOD)/ui/ui_cdkey.o \
  $(B)/$(MOD)/ui/ui_cinematics.o \
  $(B)/$(MOD)/ui/ui_confirm.o \
  $(B)/$(MOD)/ui/ui_connect.o \
  $(B)/$(MOD)/ui/ui_controls2.o \
  $(B)/$(MOD)/ui/ui_credits.o \
  $(B)/$(MOD)/ui/ui_demo2.o \
  $(B)/$(MOD)/ui/ui_display.o \
  $(B)/$(MOD)/ui/ui_gameinfo.o \
  $(B)/$(MOD)/ui/ui_ingame.o \
  $(B)/$(MOD)/ui/ui_loadconfig.o \
  $(B)/$(MOD)/ui/ui_menu.o \
  $(B)/$(MOD)/ui/ui_mfield.o \
  $(B)/$(MOD)/ui/ui_mods.o \
  $(B)/$(MOD)/ui/ui_network.o \
  $(B)/$(MOD)/ui/ui_options.o \
  $(B)/$(MOD)/ui/ui_playermodel.o \
  $(B)/$(MOD)/ui/ui_players.o \
  $(B)/$(MOD)/ui/ui_playersettings.o \
  $(B)/$(MOD)/ui/ui_preferences.o \
  $(B)/$(MOD)/ui/ui_qmenu.o \
  $(B)/$(MOD)/ui/ui_removebots.o \
  $(B)/$(MOD)/ui/ui_saveconfig.o \
  $(B)/$(MOD)/ui/ui_serverinfo.o \
  $(B)/$(MOD)/ui/ui_servers2.o \
  $(B)/$(MOD)/ui/ui_setup.o \
  $(B)/$(MOD)/ui/ui_sound.o \
  $(B)/$(MOD)/ui/ui_sparena.o \
  $(B)/$(MOD)/ui/ui_specifyserver.o \
  $(B)/$(MOD)/ui/ui_splevel.o \
  $(B)/$(MOD)/ui/ui_sppostgame.o \
  $(B)/$(MOD)/ui/ui_spskill.o \
  $(B)/$(MOD)/ui/ui_startserver.o \
  $(B)/$(MOD)/ui/ui_team.o \
  $(B)/$(MOD)/ui/ui_teamorders.o \
  $(B)/$(MOD)/ui/ui_video.o \
  \
  $(B)/$(MOD)/game/q_math.o \
  $(B)/$(MOD)/game/q_shared.o

UIOBJ = $(UIOBJ_) $(B)/$(MOD)/ui/ui_syscalls.o

$(B)/$(MOD)/ui$(SHLIBNAME): $(UIOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(UIOBJ)

#############################################################################
## GAME MODULE RULES (native)
#############################################################################

$(B)/$(MOD)/cgame/bg_%.o: $(QADIR)/bg_%.c
	$(DO_CGAME_CC)

$(B)/$(MOD)/cgame/q_%.o: $(QADIR)/q_%.c
	$(DO_CGAME_CC)

$(B)/$(MOD)/cgame/%.o: $(CGDIR)/%.c
	$(DO_CGAME_CC)

$(B)/$(MOD)/game/%.o: $(QADIR)/%.c
	$(DO_GAME_CC)

$(B)/$(MOD)/ui/bg_%.o: $(QADIR)/bg_%.c
	$(DO_UI_CC)

$(B)/$(MOD)/ui/%.o: $(UIDIR)/%.c
	$(DO_UI_CC)

#############################################################################
# MISC (native)
#############################################################################

OBJ = $(QAOBJ) $(CGOBJ) $(UIOBJ)

clean: clean-debug clean-release qvm-clean

clean-debug:
	@$(MAKE) clean2 B=$(BD)

clean-release:
	@$(MAKE) clean2 B=$(BR)

clean2:
	@echo "CLEAN $(B)"
	@rm -f $(OBJ)
	@rm -f $(TARGETS)

distclean: clean
	@rm -rf $(BUILD_DIR)

#############################################################################
# QVM build (using q3lcc/q3asm)
#############################################################################

# Output location for QVM artifacts and PK3
QVM_BUILD_DIR:=qvm
QVM_VM_DIR:=$(QVM_BUILD_DIR)/vm

# PK3 name can be overridden via environment: make qvm PK3=mymod.pk3
PK3 ?= zzzzzzzzzzzz-server.pk3

# Optional auto-copy of built PK3 to server directory
# Enable with: make qvm AUTO_COPY=1 [SERVER_COPY_DIR=/path/to/baseq3]
AUTO_COPY ?= 0
# Default preserves previous relative path used from historical linux-qvm makefile
SERVER_COPY_DIR ?= ../../../../server/baseq3

# Select q3 tools dir and zip command per platform
ifeq ($(PLATFORM),linux)
  Q3TOOLS_DIR=linux-qvm/tools
  ZIP=7z a -tzip -mx=9 -mpass=8 -mfb=255
else ifdef MINGW
  Q3TOOLS_DIR=win32-qvm/tools
  ZIP=$(Q3TOOLS_DIR)/7za.exe a -tzip -mx=9 -mpass=8 -mfb=255
else
  Q3TOOLS_DIR=linux-qvm/tools
  ZIP=7z a -tzip -mx=9 -mpass=8 -mfb=255
endif

# Q3 tool commands
Q3ASM=$(Q3TOOLS_DIR)/q3asm
Q3LCC=$(Q3TOOLS_DIR)/q3lcc -DQ3_VM -S -Wf-g -I$(QADIR)

# Flags for lcc compile stages
QA_CFLAGS = -DQAGAME

# Sources list reused from existing definition
include linux-qvm/srcs.mk

# Expanded asm list from QA_SRC: keep pre-made .asm, map others to QVM dir
qa_asm = $(foreach f,$(QA_SRC),$(if $(filter %.asm,$(f)),$(f),$(QVM_VM_DIR)/game/$(f).asm))

# Per-component compile command using q3lcc
define q3_cc
echo "Compiling: $< -> $@" && \
mkdir -p $(dir $@) && \
$(Q3LCC) $1 -o $@ $<
endef

qa_cc = $(call q3_cc,$(QA_CFLAGS))

# QVM packaging target
QVM_PKG:=$(QVM_BUILD_DIR)/$(PK3)

.PHONY: qvm qvm-dirs qvm-clean qvm-rebuild rebuild .FORCE

qvm: qvm-dirs $(QVM_PKG)
	@if [ "$(AUTO_COPY)" = "1" ]; then \
	  echo "Copying $(QVM_PKG) -> $(SERVER_COPY_DIR)/"; \
	  mkdir -p $(SERVER_COPY_DIR) && cp -f $(QVM_PKG) $(SERVER_COPY_DIR)/; \
	fi

# Full rebuild convenience target (clean + build)
qvm-rebuild: qvm-clean qvm

# Alias
rebuild: qvm-rebuild

qvm-dirs:
	mkdir -p $(QVM_VM_DIR)/game $(QVM_VM_DIR)/game/cmds $(QVM_VM_DIR)/game/svcmds $(QVM_VM_DIR)/game/cmds/osp

$(QVM_PKG): $(QVM_VM_DIR)/qagame.qvm $(FORCE_PREREQ)
	$(ZIP) $@ $(QVM_VM_DIR)/*.qvm $(QVM_VM_DIR)/*.jts
	@if [ "$(AUTO_COPY)" = "1" ]; then \
	  echo "Copying $@ -> $(SERVER_COPY_DIR)/"; \
	  mkdir -p $(SERVER_COPY_DIR) && cp -f $@ $(SERVER_COPY_DIR)/; \
	fi

$(QVM_VM_DIR)/qagame.qvm: $(qa_asm) $(FORCE_PREREQ)
	$(Q3ASM) -o $@ $(qa_asm)

# Pattern rules to build asm from C using q3lcc (fallback without explicit $< prereq)
$(QVM_VM_DIR)/game/cmds/%.asm: $(FORCE_PREREQ) | qvm-dirs
	@echo "Compiling: $(QADIR)/cmds/$*.c -> $@" && \
	mkdir -p $(dir $@) && \
	$(Q3LCC) $(QA_CFLAGS) -o $@ $(QADIR)/cmds/$*.c

$(QVM_VM_DIR)/game/svcmds/%.asm: $(FORCE_PREREQ) | qvm-dirs
	@echo "Compiling: $(QADIR)/svcmds/$*.c -> $@" && \
	mkdir -p $(dir $@) && \
	$(Q3LCC) $(QA_CFLAGS) -o $@ $(QADIR)/svcmds/$*.c

$(QVM_VM_DIR)/game/cmds/osp/%.asm: $(FORCE_PREREQ) | qvm-dirs
	@echo "Compiling: $(QADIR)/cmds/osp/$*.c -> $@" && \
	mkdir -p $(dir $@) && \
	$(Q3LCC) $(QA_CFLAGS) -o $@ $(QADIR)/cmds/osp/$*.c

$(QVM_VM_DIR)/game/%.asm: $(FORCE_PREREQ) | qvm-dirs
	@echo "Compiling: $(QADIR)/$*.c -> $@" && \
	mkdir -p $(dir $@) && \
	$(Q3LCC) $(QA_CFLAGS) -o $@ $(QADIR)/$*.c

qvm-clean:
	rm -rf $(QVM_BUILD_DIR)

# When FORCE=1 is passed, force all QVM targets to rebuild
ifeq ($(FORCE),1)
FORCE_PREREQ=.FORCE
.FORCE:
	@true
endif

#############################################################################
# Phony declaration
#############################################################################
.PHONY: all clean clean2 clean-debug clean-release \
  debug default distclean makedirs \
  release targets qvm


