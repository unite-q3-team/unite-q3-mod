include config.mk  # Include configuration settings from config.mk

# Define a function to generate source file paths
srcs = $(foreach file,$1,$(if $(patsubst %.asm,,$(file)),$2/$(file).asm,$(file)))

# Generate assembly source files for different components
qa_asm = $(call srcs,$(QA_SRC),vm/game)  # Assembly sources for the game
cg_asm = $(call srcs,$(CG_SRC),vm/cgame)  # Assembly sources for the cgame
ui_asm = $(call srcs,$(UI_SRC),vm/ui)      # Assembly sources for the UI

# Define the default target 'all' which depends on 'dirs' and the PK3 file
all: dirs $(PK3)

# Create necessary directories for the build
dirs:
	mkdir -p vm/game vm/cgame vm/ui vm/game/clcmds  # Create directories if they do not exist

.PHONY: all dirs testc  # Declare 'all', 'dirs', and 'testc' as phony targets

# Define how to create the PK3 file from the compiled QVM files
# ignore cgame here
$(PK3): vm/qagame.qvm #vm/cgame.qvm #vm/ui.qvm 
	$(7Z) $@ vm/*.qvm vm/*.jts #../../assets/*  # Use 7-Zip to package the files into a PK3

# Define a command to compile C files into assembly
cc = \
    echo "Compiling: $< -> $@" && \
    mkdir -p $(dir $@) && \
    $(Q3LCC) $2 -o $@ $<

	
# Define compilation commands for each component
qa_cc = $(call cc,game,$(QA_CFLAGS))  # Compilation command for the game
cg_cc = $(call cc,cgame,$(CG_CFLAGS))  # Compilation command for the cgame
#ui_cc = $(call cc,ui,$(UI_CFLAGS))      # Compilation command for the UI

# Define how to create QVM files from assembly sources
vm/qagame.qvm: $(qa_asm)
	$(Q3ASM) -o $@ $(qa_asm)  # Assemble game sources into QVM

# vm/cgame.qvm: $(cg_asm)	  # also ignore cgame
# 	$(Q3ASM) -o $@ $(cg_asm)  # Assemble cgame sources into QVM

#vm/ui.qvm: $(ui_asm)
#	$(Q3ASM) -o $@ $(ui_asm)  # Assemble UI sources into QVM

# Define rules for generating assembly files from C sources
vm/game/cmds/%.asm: $(QADIR)/cmds/%.c; $(qa_cc)  # Generate assembly for game/clcmds from C
vm/game/%.asm: $(QADIR)/%.c; $(qa_cc)  # Generate assembly for game from C
# vm/game/%.asm: ../../code/game/%.c; $(qa_cc)
vm/cgame/%.asm: $(QADIR)/%.c; $(cg_cc)  # Generate assembly for cgame from C
vm/cgame/%.asm: $(CGDIR)/%.c; $(cg_cc)  # Generate assembly for cgame from C in CGDIR
#vm/ui/%.asm: $(QADIR)/%.c; $(ui_cc)      # Generate assembly for UI from C
#vm/ui/%.asm: $(UIDIR)/%.c; $(ui_cc)      # Generate assembly for UI from C in UIDIR

# Define the clean target to remove generated files
clean:
	$(RM) -rf *.pk3 vm/ ../cgame/*.asm ../game/*.asm ../ui/*.asm  # Remove PK3 and assembly files

# Define the testc target to compile everything and copy the PK3 file
testc: all
	cp $(PK3) ../../../../server/baseq3/  # Copy the generated PK3 file to the specified directory
