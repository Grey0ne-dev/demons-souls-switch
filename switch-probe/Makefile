DEVKITPRO ?= $(abspath ../.tools/switch/root/opt/devkitpro)
export DEVKITPRO
CC := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc
TOOLS := $(DEVKITPRO)/tools/bin
TARGET := for-switch-memory-probe-v0.1.1
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := $(ARCH) -O2 -g -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -D__SWITCH__ -I$(DEVKITPRO)/libnx/include
LDFLAGS := $(ARCH) -specs=$(DEVKITPRO)/libnx/switch.specs -L$(DEVKITPRO)/libnx/lib -Wl,-Map,build/$(TARGET).map

.PHONY: all
all: $(TARGET).nro

build:
	mkdir -p $@

build/main.o: source/main.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/$(TARGET).elf: build/main.o
	$(CC) $(LDFLAGS) $< -lnx -o $@

build/$(TARGET).nacp: Makefile | build
	$(TOOLS)/nacptool --create "For-Switch Probe v0.1.1" "Local diagnostic" "0.1.1" $@

$(TARGET).nro: build/$(TARGET).elf build/$(TARGET).nacp
	$(TOOLS)/elf2nro $< $@ --nacp=build/$(TARGET).nacp --icon=$(DEVKITPRO)/libnx/default_icon.jpg

-include build/main.d

.PHONY: launch-check
launch-check: for-switch-launch-check.nro

build/launch_check.o: source/launch_check.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/for-switch-launch-check.elf: build/launch_check.o
	$(CC) $(ARCH) -specs=$(DEVKITPRO)/libnx/switch.specs -L$(DEVKITPRO)/libnx/lib -Wl,-Map,build/for-switch-launch-check.map $< -lnx -o $@

build/for-switch-launch-check.nacp: Makefile | build
	$(TOOLS)/nacptool --create "For-Switch Launch Check" "Local diagnostic" "0.1.0" $@

for-switch-launch-check.nro: build/for-switch-launch-check.elf build/for-switch-launch-check.nacp
	$(TOOLS)/elf2nro $< $@ --nacp=build/for-switch-launch-check.nacp --icon=$(DEVKITPRO)/libnx/default_icon.jpg

-include build/launch_check.d
