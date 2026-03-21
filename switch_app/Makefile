#---------------------------------------------------------------------------------
.SUFFIXES:

TARGET		:=	homeassistant
BUILD		:=	build
SOURCES		:=	.
INCLUDES	:=	include

#---------------------------------------------------------------------------------
DEVKITPRO	?=	/opt/devkitpro
PREFIX		:=	$(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-

CC		:=	$(PREFIX)gcc
CXX		:=	$(PREFIX)g++
LD		:=	$(PREFIX)gcc
OBJCOPY	:=	$(PREFIX)objcopy
STRIP	:=	$(PREFIX)strip
NM		:=	$(PREFIX)nm

#---------------------------------------------------------------------------------
VERSION	?=	0.2.1
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=el0 -fPIE
DEFINES	:=	-D__SWITCH__ -DAPP_VERSION=\"$(VERSION)\"
CFLAGS	:=	-g -Wall -Wextra -O2 -ffunction-sections $(ARCH) $(DEFINES) -I$(CURDIR)/include -I$(DEVKITPRO)/libnx/include -I$(DEVKITPRO)/portlibs/switch/include -I$(CURDIR)/../switch_sysmodule/include
CXXFLAGS:=	$(CFLAGS) -fno-rtti -fno-exceptions
LDFLAGS	:=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -L$(DEVKITPRO)/libnx/lib -L$(DEVKITPRO)/portlibs/switch/lib

LIBS	:= -lcurl -lmbedtls -lmbedcrypto -lmbedx509 -lz -lnx

#---------------------------------------------------------------------------------
all: $(TARGET).nro

$(TARGET).nro: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/nacptool --create "HomeAssistant Switch Integration" "FaserF" "$(VERSION)" $(TARGET).nacp
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --icon=icon.png --nacp=$(TARGET).nacp
	@echo built ... $@

$(TARGET).elf: main.cpp ../switch_sysmodule/ConfigManager.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).map
