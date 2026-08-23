#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing header files
#
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
#---------------------------------------------------------------------------------
TARGET		:=	Spotify-DS
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	include

APP_TITLE	:=	Spotify-DS
APP_DESCRIPTION	:=	Spotify companion for Nintendo 3DS
APP_AUTHOR	:=	Vital

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	:=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lctru -lm

CPPFILES	:=	$(wildcard $(SOURCES)/*.cpp)
CFILES		:=	$(wildcard $(SOURCES)/*.c)
SFILES		:=	$(wildcard $(SOURCES)/*.s)

OFILES		:=	$(CPPFILES:$(SOURCES)/%.cpp=$(BUILD)/%.o) \
			$(CFILES:$(SOURCES)/%.c=$(BUILD)/%.o) \
			$(SFILES:$(SOURCES)/%.s=$(BUILD)/%.o)

INCLUDE		:=	$(INCLUDES:%=-I%) \
			-I$(CTRULIB)/include

LIBDIR		:=	-L$(CTRULIB)/lib

export LIBCTRULIB = $(CTRULIB)/lib/libctru.a

.PHONY: all clean

#---------------------------------------------------------------------------------
all: $(BUILD) $(BUILD)/$(TARGET).3dsx

#---------------------------------------------------------------------------------
$(BUILD):
	@mkdir -p $@

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(BUILD)/$(TARGET).3dsx: $(BUILD)/$(TARGET).elf
	3dsxtool $< $@

$(BUILD)/$(TARGET).smdh: 
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(CTRULIB)/default_icon.png $@

$(BUILD)/$(TARGET).3dsx: $(BUILD)/$(TARGET).smdh

$(BUILD)/$(TARGET).elf: $(OFILES)
	arm-none-eabi-g++ $(LDFLAGS) $(LIBDIR) $(OFILES) $(LIBS) -o $@
	arm-none-eabi-nm -CSn $@ > $(BUILD)/$(TARGET).lst

#---------------------------------------------------------------------------------
# Compilation rules
#---------------------------------------------------------------------------------
$(BUILD)/%.o: $(SOURCES)/%.cpp | $(BUILD)
	arm-none-eabi-g++ -MMD -MP -MF $(BUILD)/$*.d $(INCLUDE) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	arm-none-eabi-gcc -MMD -MP -MF $(BUILD)/$*.d $(INCLUDE) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SOURCES)/%.s | $(BUILD)
	arm-none-eabi-as -g $(ARCH) -x assembler-with-cpp -c $< -o $@

-include $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# CIA packaging (requires makerom and bannertool)
# To use: make cia
# Requires: banner.png, banner.wav, build-cia.rsf, icon.png
#---------------------------------------------------------------------------------
.PHONY: cia
cia: $(BUILD)/banner.bin
	@echo "Building CIA..."
	makerom -f cia -target t -exefslogo -o $(BUILD)/$(TARGET).cia \
		-elf $(BUILD)/$(TARGET).elf -rsf build-cia.rsf \
		-banner $(BUILD)/banner.bin -icon icon.png

$(BUILD)/banner.bin: banner.png banner.wav
	@echo "Creating banner..."
	bannertool makebanner -i banner.png -a banner.wav -o $@
