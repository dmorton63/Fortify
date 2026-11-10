.PHONY: all build-kernel build-kernel-stress limine-binaries image image-stress usb-image usb-image-stress run run-stress clean FORCE

CLANG ?= clang++
LD ?= ld.lld
QEMU ?= qemu-system-x86_64
QEMU_NET_ENABLE ?= 0
QEMU_NET_DEVICE ?= e1000
QEMU_NET_FLAGS_ENABLED := -netdev user,id=net0 -device $(QEMU_NET_DEVICE),netdev=net0
QEMU_NET_FLAGS_DISABLED :=
QEMU_EXTRA_FLAGS ?=
QEMU_SERIAL_INPUT ?=
QEMU_SERIAL_INPUT_DELAY ?= 2
DISPLAY_LOGS ?= 0
FS_MOCK_READER ?= 0
SHARE_README_BRIDGE ?= 0
QEMU_SHARE_ENABLE ?= 0
QEMU_SHARE_PATH ?= $(abspath shared)
QEMU_SHARE_TAG ?= fortifyshare
SHARE_README_FILE ?=
QEMU_SHARE_FLAGS_ENABLED := -fsdev local,id=fsdev0,path=$(QEMU_SHARE_PATH),security_model=none -device virtio-9p-pci,fsdev=fsdev0,mount_tag=$(QEMU_SHARE_TAG)
QEMU_SHARE_FLAGS_DISABLED :=
ifeq ($(QEMU_NET_ENABLE),1)
QEMU_NET_FLAGS := $(QEMU_NET_FLAGS_ENABLED)
else
QEMU_NET_FLAGS := $(QEMU_NET_FLAGS_DISABLED)
endif
ifeq ($(QEMU_SHARE_ENABLE),1)
QEMU_SHARE_FLAGS := $(QEMU_SHARE_FLAGS_ENABLED)
else
QEMU_SHARE_FLAGS := $(QEMU_SHARE_FLAGS_DISABLED)
endif
XORRISO ?= xorriso
LIMINE_DEPLOY ?= limine-deploy
LIMINE_REPO ?= https://github.com/limine-bootloader/limine.git

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
ISO_ROOT := $(BUILD_DIR)/iso-root
KERNEL_OUT := $(BUILD_DIR)/fortify.elf
ISO_OUT := $(BUILD_DIR)/fortify.iso
USB_ISO_OUT := $(BUILD_DIR)/fortify-usb.iso
LINKER_SCRIPT := $(BUILD_DIR)/linker.ld
LIMINE_DIR := $(abspath $(BUILD_DIR)/limine)
LIMINE_BINARIES ?= $(abspath $(BUILD_DIR)/limine-binaries)
GENERATED_DIR := $(BUILD_DIR)/generated
SHARE_README_HEADER := $(GENERATED_DIR)/share_readme_blob.h

CPP_SOURCES := $(shell find src -type f -name '*.cpp' | sort)
ASM_SOURCES := $(shell find src -type f -name '*.S' | sort)
OBJECTS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SOURCES)) $(patsubst src/%.S,$(OBJ_DIR)/%.o,$(ASM_SOURCES))

CPPFLAGS := -Iinclude -I$(GENERATED_DIR) -DFORTIFY_DISPLAY_LOGS=$(DISPLAY_LOGS) -DFORTIFY_FS_MOCK_READER=$(FS_MOCK_READER) -DFORTIFY_SHARE_README_BRIDGE=$(SHARE_README_BRIDGE) -DFORTIFY_QEMU_SHARE_ENABLE=$(QEMU_SHARE_ENABLE) -DFORTIFY_QEMU_SHARE_TAG=\"$(QEMU_SHARE_TAG)\"
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
STRESS_CPPFLAGS := $(CPPFLAGS) -DFORTIFY_AP_STRESS=1
SHARED_LIMINE_BINARIES := $(abspath build/limine-binaries)
AP_STRESS ?= 0
ifeq ($(AP_STRESS),1)
CPPFLAGS += -DFORTIFY_AP_STRESS=1
endif

all: build-kernel

build-kernel: $(KERNEL_OUT)

build-kernel-stress:
	@$(MAKE) BUILD_DIR=build-stress LINKER_SCRIPT=build/linker.ld AP_STRESS=1 LIMINE_BINARIES='$(SHARED_LIMINE_BINARIES)' build-kernel

$(KERNEL_OUT): $(OBJECTS) $(LINKER_SCRIPT)
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built kernel: $@"

$(SHARE_README_HEADER): FORCE
	@mkdir -p "$(GENERATED_DIR)"
	@if [ "$(SHARE_README_BRIDGE)" = "1" ]; then \
		share_readme_file="$(SHARE_README_FILE)"; \
		if [ -z "$$share_readme_file" ]; then \
			if [ -f "$(QEMU_SHARE_PATH)/README.TXT" ]; then \
				share_readme_file="$(QEMU_SHARE_PATH)/README.TXT"; \
			elif [ -f "$(QEMU_SHARE_PATH)/README.txt" ]; then \
				share_readme_file="$(QEMU_SHARE_PATH)/README.txt"; \
			elif [ -f "$(QEMU_SHARE_PATH)/readme.txt" ]; then \
				share_readme_file="$(QEMU_SHARE_PATH)/readme.txt"; \
			fi; \
		fi; \
		if [ -n "$$share_readme_file" ] && [ -f "$$share_readme_file" ]; then \
			python3 -c 'import pathlib,sys;src=pathlib.Path(sys.argv[1]);dst=pathlib.Path(sys.argv[2]);data=src.read_bytes()[:4096];rows=[data[i:i+12] for i in range(0,len(data),12)];lines=["#ifndef FORTIFY_SHARE_README_BLOB_H","#define FORTIFY_SHARE_README_BLOB_H","",f"static const unsigned char fortify_share_readme_blob[{max(1,len(data))}] = {{"];lines.extend(["    0x00,"] if len(data)==0 else ["    "+", ".join(f"0x{b:02x}" for b in row)+"," for row in rows]);lines.extend(["};",f"static const unsigned int fortify_share_readme_blob_len = {len(data)}U;","","#endif"]);dst.write_text("\n".join(lines)+"\n",encoding="utf-8")' "$$share_readme_file" "$(SHARE_README_HEADER)"; \
			echo "Prepared shared README bridge blob from $$share_readme_file"; \
		else \
			echo "Warning: SHARE_README_BRIDGE=1 but no README file found under $(QEMU_SHARE_PATH). Bridge blob will be empty."; \
			printf '%s\n' \
				'#ifndef FORTIFY_SHARE_README_BLOB_H' \
				'#define FORTIFY_SHARE_README_BLOB_H' \
				'' \
				'static const unsigned char fortify_share_readme_blob[1] = {0x00};' \
				'static const unsigned int fortify_share_readme_blob_len = 0U;' \
				'' \
				'#endif' > "$(SHARE_README_HEADER)"; \
		fi; \
	else \
		printf '%s\n' \
			'#ifndef FORTIFY_SHARE_README_BLOB_H' \
			'#define FORTIFY_SHARE_README_BLOB_H' \
			'' \
			'static const unsigned char fortify_share_readme_blob[1] = {0x00};' \
			'static const unsigned int fortify_share_readme_blob_len = 0U;' \
			'' \
			'#endif' > "$(SHARE_README_HEADER)"; \
	fi

$(OBJ_DIR)/%.o: src/%.cpp $(SHARE_README_HEADER)
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

image-stress:
	@$(MAKE) BUILD_DIR=build-stress LINKER_SCRIPT=build/linker.ld AP_STRESS=1 LIMINE_BINARIES='$(SHARED_LIMINE_BINARIES)' image

usb-image: image
	@cp "$(ISO_OUT)" "$(USB_ISO_OUT)"
	@echo "Built USB-bootable image: $(USB_ISO_OUT)"

usb-image-stress: image-stress
	@cp "$(ISO_OUT)" "$(USB_ISO_OUT)"
	@echo "Built USB-bootable stress image: $(USB_ISO_OUT)"

run: image
	@command -v $(QEMU) >/dev/null 2>&1 || { echo "$(QEMU) is required to run the VM."; exit 1; }
	@if [ -n "$(QEMU_SERIAL_INPUT)" ]; then \
		( sleep "$(QEMU_SERIAL_INPUT_DELAY)"; printf "%b" "$(QEMU_SERIAL_INPUT)" ) | $(QEMU) \
			-cdrom "$(ISO_OUT)" \
			-machine q35 \
			-cpu max \
			-m 1024 \
			-smp 8 \
			$(QEMU_NET_FLAGS) \
			$(QEMU_SHARE_FLAGS) \
			$(QEMU_EXTRA_FLAGS) \
			-serial stdio; \
	else \
		if [ -t 0 ]; then \
			old_tty="$$(stty -g)"; \
			stty -echo -icanon min 1 time 0; \
			trap 'stty "$$old_tty"' EXIT INT TERM; \
			$(QEMU) \
				-cdrom "$(ISO_OUT)" \
				-machine q35 \
				-cpu max \
				-m 1024 \
				-smp 8 \
				$(QEMU_NET_FLAGS) \
				$(QEMU_SHARE_FLAGS) \
				$(QEMU_EXTRA_FLAGS) \
				-serial stdio; \
			rc="$$?"; \
			stty "$$old_tty"; \
			trap - EXIT INT TERM; \
			exit "$$rc"; \
		else \
			$(QEMU) \
				-cdrom "$(ISO_OUT)" \
				-machine q35 \
				-cpu max \
				-m 1024 \
				-smp 8 \
				$(QEMU_NET_FLAGS) \
				$(QEMU_SHARE_FLAGS) \
				$(QEMU_EXTRA_FLAGS) \
				-serial stdio; \
		fi; \
	fi

run-stress:
	@$(MAKE) BUILD_DIR=build-stress LINKER_SCRIPT=build/linker.ld AP_STRESS=1 LIMINE_BINARIES='$(SHARED_LIMINE_BINARIES)' run

clean:
	@rm -rf "$(OBJ_DIR)" "$(ISO_ROOT)" "$(KERNEL_OUT)" "$(ISO_OUT)" "$(LIMINE_BINARIES)"
	@rm -rf build-stress
