#---------------------------------------------------------------------------------
.SUFFIXES:

TARGET		:=	homeassistant_sysmodule
BUILD		:=	build
SOURCES		:=	.
INCLUDES	:=	include

#---------------------------------------------------------------------------------
PREFIX	:=	aarch64-none-elf-
CC		:=	$(PREFIX)gcc
CXX		:=	$(PREFIX)g++
LD		:=	$(PREFIX)gcc
OBJCOPY	:=	$(PREFIX)objcopy
STRIP	:=	$(PREFIX)strip
NM		:=	$(PREFIX)nm

#---------------------------------------------------------------------------------
APP_VERSION	:=	0.1.6
DEFINES	:=	-D__SWITCH__ -DAPP_VERSION=\"$(APP_VERSION)\"
CFLAGS	:=	-g -Wall -O2 -ffunction-sections -fPIE $(ARCH) $(DEFINES) -I$(CURDIR)/include -I$(DEVKITPRO)/libnx/include -I$(DEVKITPRO)/portlibs/switch/include
CXXFLAGS:=	$(CFLAGS) -fno-rtti -fno-exceptions
LDFLAGS	:=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -pie -Wl,-Map,$(notdir $*.map) -L$(DEVKITPRO)/libnx/lib -L$(DEVKITPRO)/portlibs/switch/lib

LIBS	:= -lcurl -lmbedtls -lmbedcrypto -lmbedx509 -lz -lnx

#---------------------------------------------------------------------------------
all: $(TARGET).nso

$(TARGET).nso: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2nso $< $@
	@echo built ... $@


$(TARGET).elf: main.cpp ConfigManager.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nso $(TARGET).nsp $(TARGET).map
