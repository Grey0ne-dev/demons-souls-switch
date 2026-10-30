DEVKITPRO ?= $(abspath ../.tools/switch/root/opt/devkitpro)
export DEVKITPRO
CC := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc
CXX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++
TOOLS := $(DEVKITPRO)/tools/bin
TARGET := demons-souls-loader
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CPPFLAGS := -D__SWITCH__ -Iinclude -I$(DEVKITPRO)/libnx/include
CPPFLAGS += -DDS_WITH_ENTRY -Ientry -Ibuild/entry
CFLAGS := $(ARCH) -O2 -g -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
CXXFLAGS := $(ARCH) -O2 -g -std=gnu++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fwrapv -fno-strict-aliasing -ffp-contract=off
LDFLAGS := $(ARCH) -specs=$(DEVKITPRO)/libnx/switch.specs -L$(DEVKITPRO)/libnx/lib -Wl,-Map,build/$(TARGET).map
OBJECTS := build/main.o build/ds_image.o build/ds_heap.o build/ds_atomic.o build/ds_clock.o build/ds_sync.o build/ds_rwlock.o build/ds_kernel_mutex.o build/ds_entry.o build/entry_slice.o
ENTRY_GENERATED := build/entry/entry_slice.cpp build/entry/ppu_recomp.h build/entry/entry-provenance.json
LIFT_ROOT := ../upstream/DemonsSoulsRecomp/work
LIFT_CHUNKS := $(addprefix $(LIFT_ROOT)/recomp/ppu_recomp_,000.cpp 001.cpp 002.cpp 003.cpp 004.cpp 005.cpp 006.cpp 007.cpp 010.cpp 011.cpp 012.cpp 013.cpp)

.PHONY: all reference
all: $(TARGET).nro

build:
	mkdir -p $@

build/%.o: source/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJECTS): Makefile

$(ENTRY_GENERATED) &: entry/generate_entry.py $(LIFT_ROOT)/game/EBOOT.ELF $(LIFT_ROOT)/loader/EBOOT.functions.json $(LIFT_ROOT)/recomp/ppu_recomp.h $(LIFT_CHUNKS) | build
	python3 entry/generate_entry.py --out build/entry --annotate-indirect-calls
	touch $(ENTRY_GENERATED)

build/ds_entry.o: entry/ds_entry.cpp entry/ds_entry.h build/entry/ppu_recomp.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/ds_atomic.o: entry/ds_atomic.c entry/ds_atomic.h include/ds_image.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fno-strict-aliasing -MMD -MP -c $< -o $@

build/ds_heap.o: entry/ds_heap.c entry/ds_heap.h include/ds_image.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/ds_clock.o: entry/ds_clock.cpp entry/ds_clock.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/ds_sync.o: entry/ds_sync.cpp entry/ds_sync.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/ds_kernel_mutex.o: entry/ds_kernel_mutex.cpp entry/ds_kernel_mutex.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/ds_rwlock.o: entry/ds_rwlock.cpp entry/ds_rwlock.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/entry_slice.o: build/entry/entry_slice.cpp build/entry/ppu_recomp.h | build
	# The pinned upstream bootstrap patch has two same-line if statements.
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Wno-unused-function -Wno-unused-variable -Wno-unused-label -Wno-unused-but-set-variable -Wno-misleading-indentation -MMD -MP -c $< -o $@

build/$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -lnx -o $@

build/$(TARGET).nacp: Makefile | build
	$(TOOLS)/nacptool --create "Demon's Souls Loader" "Local port development" "0.27.0" $@

$(TARGET).nro: build/$(TARGET).elf build/$(TARGET).nacp
	$(TOOLS)/elf2nro $< $@ --nacp=build/$(TARGET).nacp --icon=$(DEVKITPRO)/libnx/default_icon.jpg

-include $(OBJECTS:.o=.d)
