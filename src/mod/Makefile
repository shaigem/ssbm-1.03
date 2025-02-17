ifeq ($(VERSION), 100)
ELFFILE := $(LOCALBIN)/mod-ntsc100.elf
BINFILE := $(LOCALBIN)/mod-ntsc100.bin
else ifeq ($(VERSION), 101)
ELFFILE := $(LOCALBIN)/mod-ntsc101.elf
BINFILE := $(LOCALBIN)/mod-ntsc101.bin
else ifeq ($(VERSION), 102)
ELFFILE := $(LOCALBIN)/mod.elf
BINFILE := $(LOCALBIN)/mod.bin
else ifeq ($(VERSION), PAL)
ELFFILE := $(LOCALBIN)/mod-pal.elf
BINFILE := $(LOCALBIN)/mod-pal.bin
endif

ifdef NOPAL
ALLBINS  := --compress \
            $(LOCALBIN)/mod.bin $(LOCALBIN)/mod-ntsc100.bin.diff $(LOCALBIN)/mod-ntsc101.bin.diff
else
ALLBINS  := $(LOCALBIN)/mod.bin $(LOCALBIN)/mod-ntsc100.bin.diff $(LOCALBIN)/mod-ntsc101.bin.diff \
            $(LOCALBIN)/mod-pal.bin.diff
endif

GCINAME   := 103Code
GCIFILE   := $(BINDIR)/$(MODNAME).gci
BANNERSRC := banner.rgb5a3.png
BANNER    := banner.tex
ICON      := icon.tex
TITLE     := "Super Smash Bros. Melee         1.03 Data"
DIFFSRC   := $(LOCALBIN)/mod.bin

DOLFILE    := $(ISODIR)/sys/main.dol
PATCHES    := $(LOCALBIN)/patches.bin
DOLELF     := $(LOCALBIN)/dol.elf
DOLDATA    := $(OBJDIR)/dol_data.bin
DOLLD      := dol.ld

CFILES   := $(foreach dir, $(SRCDIR), $(shell find $(dir) -type f -name '*.c'   2> /dev/null))
CXXFILES := $(foreach dir, $(SRCDIR), $(shell find $(dir) -type f -name '*.cpp' 2> /dev/null))
SFILES   := $(foreach dir, $(SRCDIR), $(shell find $(dir) -type f -name '*.S'   2> /dev/null))

OBJFILES := \
    $(patsubst %, $(OBJDIR)/%.o, $(CFILES)) \
    $(patsubst %, $(OBJDIR)/%.o, $(CXXFILES)) \
    $(patsubst %, $(OBJDIR)/%.o, $(SFILES))

DEPFILES := $(patsubst $(OBJDIR)/%.o, $(DEPDIR)/%.d, $(OBJFILES))

LINKSCRIPT := mod.ld

CFLAGS    += -flto
CXXFLAGS  += -flto
LDFLAGS   += -flto

ifeq ($(VERSION), 102)
$(GCIFILE): $(BINFILE) $(BANNER) $(ICON) $(TOOLS)/bin_to_gci.py
	@[ -d $(@D) ] || mkdir -p $(@D)
	python $(TOOLS)/bin_to_gci.py $@ $(GCINAME) $(TITLE) $(BANNER) $(ICON) $(ALLBINS)
else
$(GCIFILE): $(BINFILE).diff $(BANNER) $(ICON) $(TOOLS)/bin_to_gci.py
	@[ -d $(@D) ] || mkdir -p $(@D)
	python $(TOOLS)/bin_to_gci.py $@ $(GCINAME) $(TITLE) $(BANNER) $(ICON) $(ALLBINS)

$(BINFILE).diff: $(BINFILE) $(TOOLS)/diff.py
	@[ -d $(@D) ] || mkdir -p $(@D)
	python $(TOOLS)/diff.py $(DIFFSRC) $< $@
endif

.PHONY: dol
dol: $(DOLFILE)

$(DOLFILE): $(DOLELF) $(PATCHES) $(TOOLS)/patch_dol.py
	@[ -d $(@D) ] || mkdir -p $(@D)
	@[ $(DOLSRC) ] || ( echo "\$$DOLSRC must be set to the path of an NTSC 1.02 main.dol file." >& 2; exit 1 )
	$(OBJCOPY) -O binary -R .patches $< $@
	python $(TOOLS)/patch_dol.py $@ $@ $(OUTPUTMAP) $(PATCHES)

$(PATCHES): $(DOLELF)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(OBJCOPY) -O binary -j .patches $< $@

$(DOLELF): $(OBJFILES) $(DOLDATA) $(DOLLD) $(MELEELD) ../common.ld | clean-unused
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(LDFLAGS) -T$(DOLLD) -T$(MELEELD) $(OBJFILES) -o $@

$(DOLDATA): $(DOLSRC)
	@[ -d $(@D) ] || mkdir -p $(@D)
# skip dol header
	dd if=$(DOLSRC) of=$(DOLDATA) bs=256 skip=1

$(BANNER): $(BANNERSRC) $(TOOLS)/encode_texture.py
	python $(TOOLS)/encode_texture.py $< $@

$(BINFILE): $(ELFFILE)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(OBJCOPY) -O binary $< $@
ifeq ($(VERSION), 102)
# diffs are no longer valid
	rm -f $(LOCALBIN)/mod-ntsc100.bin.diff
	rm -f $(LOCALBIN)/mod-ntsc101.bin.diff
	rm -f $(LOCALBIN)/mod-pal.bin.diff
endif

$(ELFFILE): $(OBJFILES) $(LINKSCRIPT) $(MELEELD) ../common.ld | clean-unused
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(LDFLAGS) -T$(LINKSCRIPT) -T$(MELEELD) $(OBJFILES) -o $@

$(OBJDIR)/%.c.o: %.c | resources
	@[ -d $(@D) ] || mkdir -p $(@D)
	@[ -d $(subst $(OBJDIR), $(DEPDIR), $(@D)) ] || mkdir -p $(subst $(OBJDIR), $(DEPDIR), $(@D))
	$(CC) -MMD -MP -MF $(patsubst $(OBJDIR)/%.o, $(DEPDIR)/%.d, $@) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(OBJDIR)/%.cpp.o: %.cpp | resources
	@[ -d $(@D) ] || mkdir -p $(@D)
	@[ -d $(subst $(OBJDIR), $(DEPDIR), $(@D)) ] || mkdir -p $(subst $(OBJDIR), $(DEPDIR), $(@D))
	$(CXX) -MMD -MP -MF $(patsubst $(OBJDIR)/%.o, $(DEPDIR)/%.d, $@) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(OBJDIR)/%.S.o: %.S
	@[ -d $(@D) ] || mkdir -p $(@D)
	@[ -d $(subst $(OBJDIR), $(DEPDIR), $(@D)) ] || mkdir -p $(subst $(OBJDIR), $(DEPDIR), $(@D))
	$(CC) $(ASFLAGS) -c $< -o $@

RESOURCE_DIR_IN  := resources
RESOURCE_DIR_OUT := $(GENDIR)/resources
-include resources.mk

.PHONY: clean
clean:
	rm -rf build

# Remove unused obj/dep/gen files
.PHONY: clean-unused
clean-unused:
	$(foreach file, $(shell find $(OBJDIR) -type f 2> /dev/null), \
		$(if $(filter $(file), $(OBJFILES) $(OUTPUTMAP) $(DOLDATA)),, \
		rm $(file);))
	$(foreach file, $(shell find $(DEPDIR) -type f 2> /dev/null), \
		$(if $(filter $(file), $(DEPFILES)),, \
		rm $(file);))
	$(foreach file, $(shell find $(GENDIR) -type f 2> /dev/null), \
		$(if $(filter $(file), $(RESOURCES_OUT) $(RESOURCE_HEADERS)),, \
		rm $(file);))

-include $(DEPFILES)