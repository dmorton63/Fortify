.PHONY: all build-kernel limine-binaries image run clean

CLANG ?= clang++
LD ?= ld.lld
QEMU ?= qemu-system-x86_64
XORRISO ?= xorriso
LIMINE_DEPLOY ?= limine-deploy
LIMINE_REPO ?= https://github.com/limine-bootloader/limine.git

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
ISO_ROOT := $(BUILD_DIR)/iso-root
KERNEL_OUT := $(BUILD_DIR)/fortify.elf
ISO_OUT := $(BUILD_DIR)/fortify.iso
LINKER_SCRIPT := $(BUILD_DIR)/linker.ld
LIMINE_DIR := $(abspath $(BUILD_DIR)/limine)
LIMINE_BINARIES ?= $(abspath $(BUILD_DIR)/limine-binaries)

CPP_SOURCES := $(shell find src -type f -name '*.cpp' | sort)
ASM_SOURCES := $(shell find src -type f -name '*.S' | sort)
OBJECTS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SOURCES)) $(patsubst src/%.S,$(OBJ_DIR)/%.o,$(ASM_SOURCES))

CPPFLAGS := -Iinclude
CXXFLAGS := \
	-target x86_64-unknown-none \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-mcmodel=kernel \
	-mno-red-zone \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-mno-avx \
	-mno-avx2 \
	-Wall \
	-Wextra \
	-O2 \
	-fno-exceptions \
	-fno-rtti \
	-fno-use-cxa-atexit \
	-fno-threadsafe-statics \
	-fno-aligned-allocation
ASMFLAGS := \
	-target x86_64-unknown-none \
	-ffreestanding \
	-fno-pic \
	-mno-red-zone
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T $(LINKER_SCRIPT)

all: build-kernel

build-kernel: $(KERNEL_OUT)

$(KERNEL_OUT): $(OBJECTS) $(LINKER_SCRIPT)
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built kernel: $@"

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	@$(CLANG) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: src/%.S
	@mkdir -p $(dir $@)
	@$(CLANG) $(CPPFLAGS) $(ASMFLAGS) -c $< -o $@

limine-binaries:
	@set -e; \
	if [ -f "$(LIMINE_BINARIES)/limine-bios.sys" ] && \
		[ -f "$(LIMINE_BINARIES)/limine-bios-cd.bin" ] && \
		[ -f "$(LIMINE_BINARIES)/limine-uefi-cd.bin" ] && \
		[ -f "$(LIMINE_BINARIES)/BOOTX64.EFI" ] && \
		[ -f "$(LIMINE_BINARIES)/BOOTIA32.EFI" ]; then \
		echo "Using existing Limine binaries from $(LIMINE_BINARIES)"; \
	else \
		if [ ! -d "$(LIMINE_DIR)/.git" ]; then \
			echo "Cloning Limine into $(LIMINE_DIR)"; \
			git clone --depth 1 "$(LIMINE_REPO)" "$(LIMINE_DIR)"; \
		fi; \
		if [ ! -f "$(LIMINE_DIR)/Makefile" ]; then \
			echo "Bootstrapping Limine build system"; \
			cd "$(LIMINE_DIR)" && ./bootstrap; \
		fi; \
		cd "$(LIMINE_DIR)" && ./configure \
			--enable-bios \
			--enable-bios-cd \
			--enable-uefi-x86-64 \
			--enable-uefi-ia32 \
			--enable-uefi-cd; \
		$(MAKE) -C "$(LIMINE_DIR)"; \
		mkdir -p "$(LIMINE_BINARIES)"; \
		cp "$(LIMINE_DIR)/bin/limine-bios.sys" "$(LIMINE_BINARIES)/limine-bios.sys"; \
		cp "$(LIMINE_DIR)/bin/limine-bios-cd.bin" "$(LIMINE_BINARIES)/limine-bios-cd.bin"; \
		cp "$(LIMINE_DIR)/bin/limine-uefi-cd.bin" "$(LIMINE_BINARIES)/limine-uefi-cd.bin"; \
		cp "$(LIMINE_DIR)/bin/BOOTX64.EFI" "$(LIMINE_BINARIES)/BOOTX64.EFI"; \
		cp "$(LIMINE_DIR)/bin/BOOTIA32.EFI" "$(LIMINE_BINARIES)/BOOTIA32.EFI"; \
		cp "$(LIMINE_DIR)/bin/limine" "$(LIMINE_BINARIES)/limine-deploy"; \
		echo "Prepared Limine binaries in $(LIMINE_BINARIES)"; \
	fi

image: build-kernel limine-binaries
	@command -v $(XORRISO) >/dev/null 2>&1 || { echo "$(XORRISO) is required for ISO creation."; exit 1; }
	@mkdir -p "$(ISO_ROOT)/boot" "$(ISO_ROOT)/EFI/BOOT"
	@cp "$(KERNEL_OUT)" "$(ISO_ROOT)/boot/fortify.elf"
	@cp "boot/limine.conf" "$(ISO_ROOT)/boot/limine.conf"
	@cp "$(LIMINE_BINARIES)/limine-bios.sys" "$(ISO_ROOT)/boot/limine-bios.sys"
	@cp "$(LIMINE_BINARIES)/limine-bios-cd.bin" "$(ISO_ROOT)/boot/limine-bios-cd.bin"
	@cp "$(LIMINE_BINARIES)/limine-uefi-cd.bin" "$(ISO_ROOT)/boot/limine-uefi-cd.bin"
	@cp "$(LIMINE_BINARIES)/BOOTX64.EFI" "$(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI"
	@cp "$(LIMINE_BINARIES)/BOOTIA32.EFI" "$(ISO_ROOT)/EFI/BOOT/BOOTIA32.EFI"
	@$(XORRISO) -as mkisofs \
		-b boot/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		"$(ISO_ROOT)" \
		-o "$(ISO_OUT)"
	@if command -v "$(LIMINE_DEPLOY)" >/dev/null 2>&1; then \
		if "$(LIMINE_DEPLOY)" --help 2>/dev/null | grep -q "bios-install"; then \
			"$(LIMINE_DEPLOY)" bios-install "$(ISO_OUT)"; \
		else \
			"$(LIMINE_DEPLOY)" "$(ISO_OUT)"; \
		fi; \
	elif [ -x "$(LIMINE_BINARIES)/limine-deploy" ]; then \
		if "$(LIMINE_BINARIES)/limine-deploy" --help 2>/dev/null | grep -q "bios-install"; then \
			"$(LIMINE_BINARIES)/limine-deploy" bios-install "$(ISO_OUT)"; \
		else \
			"$(LIMINE_BINARIES)/limine-deploy" "$(ISO_OUT)"; \
		fi; \
	else \
		echo "Warning: limine-deploy not found in PATH or $(LIMINE_BINARIES). Skipping limine deploy step."; \
	fi
	@echo "Built image: $(ISO_OUT)"

run: image
	@command -v $(QEMU) >/dev/null 2>&1 || { echo "$(QEMU) is required to run the VM."; exit 1; }
	@$(QEMU) \
		-cdrom "$(ISO_OUT)" \
		-machine q35 \
		-cpu max \
		-m 1024 \
		-smp 4 \
		-serial stdio

clean:
	@rm -rf "$(OBJ_DIR)" "$(ISO_ROOT)" "$(KERNEL_OUT)" "$(ISO_OUT)" "$(LIMINE_BINARIES)"
