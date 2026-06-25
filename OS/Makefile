TARGET := x86_64-elf

BUILD_DIR := build
KERNEL := $(BUILD_DIR)/zenithos.elf
ISO := $(BUILD_DIR)/zenithos.iso
ISO_ROOT := $(BUILD_DIR)/iso_root
RUN_OVMF := $(BUILD_DIR)/edk2-x86_64-code.fd
RUN_OVMF_VARS := $(BUILD_DIR)/edk2-vars.fd
WORKSTATION_ROOTFS := $(BUILD_DIR)/workstation/rootfs
WORKSTATION_SEED_ISO := $(BUILD_DIR)/workstation/zenithos-seed.iso
WORKSTATION_PERSISTENCE_DISK := $(BUILD_DIR)/workstation/zenithos-persistence.raw
WORKSTATION_INSTALL_DISK := $(BUILD_DIR)/workstation/zenithos-install-target.qcow2
WORKSTATION_LIVE_KERNEL := $(BUILD_DIR)/workstation/iso-stage/live/vmlinuz
WORKSTATION_LIVE_INITRD := $(BUILD_DIR)/workstation/iso-stage/live/initrd.img
WORKSTATION_LIVE_ARGS := boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us live-config.noautologin live-config.nox11autologin console=tty0 console=ttyS0,115200n8 systemd.show_status=1 loglevel=4 plymouth.enable=0 zenith.boot_report=1 zenith.profile=dev-vm
WORKSTATION_SPLASH_ARGS := boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us live-config.noautologin live-config.nox11autologin quiet splash loglevel=3 systemd.show_status=0 plymouth.ignore-serial-consoles zenith.profile=dev-vm
WORKSTATION_DEBUG_ARGS := boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us live-config.noautologin live-config.nox11autologin console=tty0 console=ttyS0,115200n8 systemd.show_status=1 loglevel=4 zenith.boot_report=1 zenith.profile=dev-vm

ifeq ($(OS),Windows_NT)
WIN_USERPROFILE := $(subst \,/,$(USERPROFILE))
CLANG64_BIN := C:/msys64/clang64/bin
ifeq ($(origin CC),default)
ifneq ($(wildcard $(CLANG64_BIN)/clang.exe),)
CC := $(CLANG64_BIN)/clang.exe
else
CC := clang
endif
endif
ifeq ($(origin LD),default)
ifneq ($(wildcard $(CLANG64_BIN)/ld.lld.exe),)
LD := $(CLANG64_BIN)/ld.lld.exe
else
LD := ld.lld
endif
endif
ifneq ($(wildcard $(CLANG64_BIN)/llvm-objcopy.exe),)
OBJCOPY ?= $(CLANG64_BIN)/llvm-objcopy.exe
else
OBJCOPY ?= llvm-objcopy
endif
PYTHON ?= python
TOOLCHAIN_CFLAGS := --target=x86_64-unknown-elf
LINK := $(LD)
TOOLCHAIN_LDFLAGS := -m elf_x86_64
XORRISO ?= C:/msys64/usr/bin/xorriso.exe
LIMINE ?= $(WIN_USERPROFILE)/.local/tools/limine-v11.4.0-binary/limine.exe
LIMINE_DIR ?= $(WIN_USERPROFILE)/.local/tools/limine-v11.4.0-binary
OVMF_CODE ?= C:/Program Files/qemu/share/edk2-x86_64-code.fd
OVMF_VARS ?= C:/Program Files/qemu/share/edk2-i386-vars.fd
else
ifeq ($(origin CC),default)
CC := $(TARGET)-gcc
endif
LINK := $(CC)
TOOLCHAIN_CFLAGS :=
TOOLCHAIN_LDFLAGS :=
OBJCOPY ?= $(TARGET)-objcopy
PYTHON ?= python3
XORRISO ?= xorriso
LIMINE ?= limine
LIMINE_DIR ?= /usr/share/limine
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd
endif

CFLAGS := $(TOOLCHAIN_CFLAGS) -std=c11 -ffreestanding -O2 -Wall -Wextra -Wpedantic
CFLAGS += -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2
CFLAGS += -fno-stack-protector -fno-pic -fno-pie -I kernel/include -I $(BUILD_DIR)/kernel/fs

ASFLAGS := $(CFLAGS)

USER_CFLAGS := $(TOOLCHAIN_CFLAGS) -std=c11 -ffreestanding -O2 -Wall -Wextra -Wpedantic
USER_CFLAGS += -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2
USER_CFLAGS += -fno-stack-protector -fno-pic -fno-pie -fno-builtin -I userland/include
USER_ASFLAGS := $(USER_CFLAGS)

LDFLAGS := $(TOOLCHAIN_LDFLAGS) -nostdlib -static -T kernel/linker.ld
USER_LDFLAGS := $(TOOLCHAIN_LDFLAGS) -nostdlib -static -T userland/linker.ld
ifeq ($(OS),Windows_NT)
LDFLAGS += -z max-page-size=0x1000
USER_LDFLAGS += -z max-page-size=0x1000
else
LDFLAGS += -Xlinker -z -Xlinker max-page-size=0x1000
USER_LDFLAGS += -Xlinker -z -Xlinker max-page-size=0x1000
endif

ifeq ($(OS),Windows_NT)
SHELL := powershell.exe
.SHELLFLAGS := -NoProfile -ExecutionPolicy Bypass -Command
endif

SOURCES_C := \
	kernel/main.c \
	kernel/panic.c \
	kernel/fs/initramfs.c \
	kernel/input/input.c \
	kernel/input/keyboard.c \
	kernel/input/mouse.c \
	kernel/ipc/ipc.c \
	kernel/ipc/ui.c \
	kernel/lib/mem.c \
	kernel/sched/scheduler.c \
	kernel/sys/syscall.c \
	kernel/user/loader.c \
	kernel/time/timer.c \
	kernel/mm/kheap.c \
	kernel/mm/pmm.c \
	kernel/mm/vmm.c \
	kernel/arch/x86_64/cpu.c \
	kernel/arch/x86_64/interrupts.c \
	kernel/arch/x86_64/serial.c \
	kernel/gfx/console.c \
	kernel/gfx/framebuffer.c

SOURCES_S := \
	kernel/arch/x86_64/entry.S \
	kernel/arch/x86_64/interrupt_stubs.S

OBJECTS := \
	$(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES_C)) \
	$(patsubst %.S,$(BUILD_DIR)/%.o,$(SOURCES_S))

USER_APPS := init session terminal settings launcher sh help sysinfo echo
USER_COMMON_OBJECTS := \
	$(BUILD_DIR)/userland/lib/start.o \
	$(BUILD_DIR)/userland/lib/syscall.o
USER_OBJECTS := $(addprefix $(BUILD_DIR)/userland/,$(addsuffix .o,$(USER_APPS))) $(USER_COMMON_OBJECTS)
USER_ELFS := $(addprefix $(BUILD_DIR)/userland/,$(addsuffix .elf,$(USER_APPS)))
USER_BINS := $(addprefix $(BUILD_DIR)/userland/,$(addsuffix .bin,$(USER_APPS)))
USERLAND_INC := $(BUILD_DIR)/kernel/fs/initramfs_userland.inc

.PHONY: all clean toolchain-check iso run kernel-lab kernel-lab-iso kernel-lab-run workstation-check workstation-runtime-check workstation-runtime-check-wsl workstation-packages workstation-packages-wsl workstation-repo workstation-repo-wsl workstation-verify-repo-wsl workstation-apt-check-wsl workstation-rootfs workstation-rootfs-wsl workstation-seed-iso workstation-seed-iso-wsl workstation-vm-persistence workstation-vm-persistence-wsl workstation-vm-install-disk workstation-autoinstall-disk workstation-autoinstall-manual-login-disk workstation-smoke-seed workstation-smoke-seed-persistent workstation-smoke-installed-disk workstation-smoke-installed-manual-login workstation-run-seed workstation-run-seed-persistent workstation-run-seed-splash workstation-run-seed-iso workstation-run-seed-install-target workstation-run-installed-disk workstation-run-seed-fast workstation-run-seed-debug workstation-diagnose-qemu
.SECONDARY: $(USER_OBJECTS) $(USER_ELFS) $(USER_BINS)

all: $(KERNEL)

kernel-lab: all

kernel-lab-iso: iso

kernel-lab-run: run

workstation-check:
	$(PYTHON) tools/check_workstation.py

workstation-runtime-check: workstation-check
ifeq ($(OS),Windows_NT)
	@Write-Error 'workstation-runtime-check is a Linux/ZenithOS target. Use workstation-runtime-check-wsl from Windows bootstrap hosts.'; exit 1
else
	bash workstation/qa/runtime-check.sh
endif

workstation-runtime-check-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -- bash -lc "cd '$$repo' && bash workstation/qa/runtime-check.sh"
else
	bash workstation/qa/runtime-check.sh
endif

workstation-packages: workstation-check
ifeq ($(OS),Windows_NT)
	@Write-Error 'workstation-packages is a Linux/ZenithOS package-build target. Use workstation-packages-wsl only for the one-time Windows bootstrap.'; exit 1
else
	bash workstation/distro/build-zenith-packages.sh
endif

workstation-packages-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -- bash -lc "cd '$$repo' && export ZENITH_PACKAGE_WORK='/tmp/zenithos-package-work' && bash workstation/distro/build-zenith-packages.sh"
else
	@echo 'workstation-packages-wsl is only needed from Windows bootstrap hosts.'
endif

workstation-repo: workstation-check
ifeq ($(OS),Windows_NT)
	@Write-Error 'workstation-repo is a Linux/ZenithOS package-repo target. Use workstation-repo-wsl only for the one-time Windows bootstrap.'; exit 1
else
	bash workstation/distro/build-zenith-packages.sh
	bash workstation/distro/publish-apt-repo.sh
endif

workstation-repo-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -- bash -lc "cd '$$repo' && export ZENITH_PACKAGE_WORK='/tmp/zenithos-package-work' ZENITH_APT_SIGNING_KEY='86DB09CA6DAE6823' && bash workstation/distro/build-zenith-packages.sh && bash workstation/distro/publish-apt-repo.sh"
else
	@echo 'workstation-repo-wsl is only needed from Windows bootstrap hosts.'
endif

workstation-verify-repo-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -- bash -lc "cd '$$repo' && bash workstation/distro/verify-apt-repo.sh"
else
	bash workstation/distro/verify-apt-repo.sh
endif

workstation-apt-check-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -- bash -lc "cd '$$repo' && bash workstation/iso/check-apt-packages.sh"
else
	bash workstation/iso/check-apt-packages.sh
endif

workstation-rootfs: workstation-check
ifeq ($(OS),Windows_NT)
	@Write-Error 'workstation-rootfs is a Linux/ZenithOS self-host target. Boot ZenithOS and run zenith-build rootfs, or use workstation-rootfs-wsl only for the one-time seed bootstrap.'; exit 1
else
	bash workstation/iso/build-rootfs.sh $(WORKSTATION_ROOTFS)
endif

workstation-rootfs-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -u root -- bash -lc "cd '$$repo' && bash workstation/iso/build-rootfs.sh"
else
	@echo 'workstation-rootfs-wsl is only needed from Windows bootstrap hosts.'
endif

workstation-seed-iso: workstation-check
ifeq ($(OS),Windows_NT)
	@Write-Error 'workstation-seed-iso is a Linux/ZenithOS target. Use workstation-seed-iso-wsl only for the one-time Windows seed bootstrap.'; exit 1
else
	bash workstation/iso/build-seed-iso.sh $(WORKSTATION_SEED_ISO)
endif

workstation-seed-iso-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -u root -- bash -lc "cd '$$repo' && export ZENITH_APT_SIGNING_KEY='builders@zenithos.local' && bash workstation/iso/build-seed-iso.sh build/workstation/zenithos-seed.iso"
else
	@echo 'workstation-seed-iso-wsl is only needed from Windows bootstrap hosts.'
endif

workstation-vm-persistence: workstation-check
ifeq ($(OS),Windows_NT)
	@Write-Error 'workstation-vm-persistence is a Linux/ZenithOS target. Use workstation-vm-persistence-wsl from Windows bootstrap hosts.'; exit 1
else
	bash workstation/iso/create-persistence-disk.sh $(WORKSTATION_PERSISTENCE_DISK)
endif

workstation-vm-persistence-wsl: workstation-check
ifeq ($(OS),Windows_NT)
	$$repo = (wsl -d Ubuntu -- wslpath -a '$(CURDIR)').Trim(); wsl -d Ubuntu -- bash -lc "cd '$$repo' && bash workstation/iso/create-persistence-disk.sh build/workstation/zenithos-persistence.raw"
else
	@echo 'workstation-vm-persistence-wsl is only needed from Windows bootstrap hosts.'
endif

workstation-vm-install-disk: workstation-check
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/create_workstation_install_disk.ps1
else
	@mkdir -p "$(BUILD_DIR)/workstation"
	qemu-img create -f qcow2 "$(WORKSTATION_INSTALL_DISK)" 40G
endif

workstation-smoke-seed: workstation-check
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/smoke_workstation_seed.ps1 -Seconds 90
else
	@echo 'workstation-smoke-seed is currently implemented for the Windows QEMU host.'
endif

workstation-smoke-seed-persistent: workstation-check
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/smoke_workstation_seed.ps1 -Seconds 90 -Persistent
else
	@echo 'workstation-smoke-seed-persistent is currently implemented for the Windows QEMU host.'
endif

workstation-smoke-installed-disk: workstation-check
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/smoke_workstation_seed.ps1 -Seconds 240 -InstalledDisk
else
	@echo 'workstation-smoke-installed-disk is currently implemented for the Windows QEMU host.'
endif

workstation-smoke-installed-manual-login: workstation-check
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/smoke_workstation_seed.ps1 -Seconds 260 -ManualLogin
else
	@echo 'workstation-smoke-installed-manual-login is currently implemented for the Windows QEMU host.'
endif

workstation-run-seed:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode direct
else
	@test -f "$(WORKSTATION_SEED_ISO)" || { echo "missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_KERNEL)" || { echo "missing $(WORKSTATION_LIVE_KERNEL); rebuild with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_INITRD)" || { echo "missing $(WORKSTATION_LIVE_INITRD); rebuild with make workstation-seed-iso first"; exit 1; }
	@mkdir -p "$(BUILD_DIR)/workstation"
	@rm -f "$(BUILD_DIR)/workstation/seed-serial.log" "$(BUILD_DIR)/workstation/qemu-stderr.log"
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-cdrom "$(WORKSTATION_SEED_ISO)" \
		-kernel "$(WORKSTATION_LIVE_KERNEL)" \
		-initrd "$(WORKSTATION_LIVE_INITRD)" \
		-append "$(WORKSTATION_LIVE_ARGS)" \
		-display gtk,gl=off \
		-serial file:$(BUILD_DIR)/workstation/seed-serial.log -monitor none \
		2> "$(BUILD_DIR)/workstation/qemu-stderr.log"
endif

workstation-run-seed-persistent:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode persistent
else
	@test -f "$(WORKSTATION_SEED_ISO)" || { echo "missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_KERNEL)" || { echo "missing $(WORKSTATION_LIVE_KERNEL); rebuild with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_INITRD)" || { echo "missing $(WORKSTATION_LIVE_INITRD); rebuild with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_PERSISTENCE_DISK)" || { echo "missing $(WORKSTATION_PERSISTENCE_DISK); build it with make workstation-vm-persistence first"; exit 1; }
	@mkdir -p "$(BUILD_DIR)/workstation"
	@rm -f "$(BUILD_DIR)/workstation/seed-serial.log" "$(BUILD_DIR)/workstation/qemu-stderr.log"
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-drive file="$(WORKSTATION_PERSISTENCE_DISK)",if=virtio,format=raw,cache=writeback \
		-cdrom "$(WORKSTATION_SEED_ISO)" \
		-kernel "$(WORKSTATION_LIVE_KERNEL)" \
		-initrd "$(WORKSTATION_LIVE_INITRD)" \
		-append "$(WORKSTATION_LIVE_ARGS) persistence" \
		-display gtk,gl=off \
		-serial file:$(BUILD_DIR)/workstation/seed-serial.log -monitor none \
		2> "$(BUILD_DIR)/workstation/qemu-stderr.log"
endif

workstation-run-seed-splash:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode splash
else
	@test -f "$(WORKSTATION_SEED_ISO)" || { echo "missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_KERNEL)" || { echo "missing $(WORKSTATION_LIVE_KERNEL); rebuild with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_INITRD)" || { echo "missing $(WORKSTATION_LIVE_INITRD); rebuild with make workstation-seed-iso first"; exit 1; }
	@mkdir -p "$(BUILD_DIR)/workstation"
	@rm -f "$(BUILD_DIR)/workstation/seed-serial-splash.log" "$(BUILD_DIR)/workstation/qemu-splash-stderr.log"
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-cdrom "$(WORKSTATION_SEED_ISO)" \
		-kernel "$(WORKSTATION_LIVE_KERNEL)" \
		-initrd "$(WORKSTATION_LIVE_INITRD)" \
		-append "$(WORKSTATION_SPLASH_ARGS)" \
		-display gtk,gl=off \
		-serial file:$(BUILD_DIR)/workstation/seed-serial-splash.log -monitor none \
		2> "$(BUILD_DIR)/workstation/qemu-splash-stderr.log"
endif

workstation-run-seed-iso:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode iso
else
	@test -f "$(WORKSTATION_SEED_ISO)" || { echo "missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso first"; exit 1; }
	@mkdir -p "$(BUILD_DIR)/workstation"
	@rm -f "$(BUILD_DIR)/workstation/seed-serial.log"
	@rm -f "$(BUILD_DIR)/workstation/qemu-stderr.log"
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file="$(OVMF_VARS)" \
		-cdrom $(WORKSTATION_SEED_ISO) -boot d -display gtk,gl=off \
		-serial file:$(BUILD_DIR)/workstation/seed-serial.log -monitor none \
		2> "$(BUILD_DIR)/workstation/qemu-stderr.log"
endif

workstation-run-seed-install-target:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode install-target
else
	@test -f "$(WORKSTATION_SEED_ISO)" || { echo "missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_INSTALL_DISK)" || { echo "missing $(WORKSTATION_INSTALL_DISK); build it with make workstation-vm-install-disk first"; exit 1; }
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file="$(OVMF_VARS)" \
		-drive file="$(WORKSTATION_INSTALL_DISK)",if=virtio,format=qcow2,cache=writeback \
		-cdrom "$(WORKSTATION_SEED_ISO)" \
		-boot d \
		-display gtk,gl=off \
		-serial file:$(BUILD_DIR)/workstation/seed-install-target-serial.log -monitor none
endif

workstation-autoinstall-disk:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode autoinstall -Wait
else
	@echo 'workstation-autoinstall-disk is currently implemented for the Windows QEMU host.'
endif

workstation-autoinstall-manual-login-disk:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode autoinstall-manual-login -Wait
else
	@echo 'workstation-autoinstall-manual-login-disk is currently implemented for the Windows QEMU host.'
endif

workstation-run-installed-disk:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode installed
else
	@test -f "$(WORKSTATION_INSTALL_DISK)" || { echo "missing $(WORKSTATION_INSTALL_DISK); install ZenithOS to it first"; exit 1; }
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file="$(OVMF_VARS)" \
		-drive file="$(WORKSTATION_INSTALL_DISK)",if=virtio,format=qcow2,cache=writeback \
		-boot order=c,strict=on \
		-display gtk,gl=off \
		-serial file:$(BUILD_DIR)/workstation/installed-disk-serial.log -monitor none
endif

workstation-run-seed-fast:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_workstation_seed.ps1 -Mode fast
else
	@echo 'workstation-run-seed-fast uses Windows WHPX; use workstation-run-seed on this host.'
endif

workstation-run-seed-debug:
ifeq ($(OS),Windows_NT)
	@if (-not (Test-Path '$(WORKSTATION_SEED_ISO)')) { Write-Error 'missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso-wsl first'; exit 1 }
	@if (-not (Test-Path '$(WORKSTATION_LIVE_KERNEL)')) { Write-Error 'missing $(WORKSTATION_LIVE_KERNEL); rebuild with make workstation-seed-iso-wsl first'; exit 1 }
	@if (-not (Test-Path '$(WORKSTATION_LIVE_INITRD)')) { Write-Error 'missing $(WORKSTATION_LIVE_INITRD); rebuild with make workstation-seed-iso-wsl first'; exit 1 }
	@New-Item -ItemType Directory -Force -Path '$(BUILD_DIR)/workstation' | Out-Null
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 -accel tcg,thread=multi -vga virtio -device virtio-rng-pci -device qemu-xhci -device usb-tablet -cdrom $(WORKSTATION_SEED_ISO) -kernel $(WORKSTATION_LIVE_KERNEL) -initrd $(WORKSTATION_LIVE_INITRD) -append '$(WORKSTATION_DEBUG_ARGS)' -display gtk,gl=off -serial stdio -monitor none -no-reboot
else
	@test -f "$(WORKSTATION_SEED_ISO)" || { echo "missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_KERNEL)" || { echo "missing $(WORKSTATION_LIVE_KERNEL); rebuild with make workstation-seed-iso first"; exit 1; }
	@test -f "$(WORKSTATION_LIVE_INITRD)" || { echo "missing $(WORKSTATION_LIVE_INITRD); rebuild with make workstation-seed-iso first"; exit 1; }
	qemu-system-x86_64 -machine q35 -m 4096M -cpu max -smp 2 \
		-accel tcg,thread=multi \
		-vga virtio \
		-device virtio-rng-pci \
		-device qemu-xhci -device usb-tablet \
		-cdrom "$(WORKSTATION_SEED_ISO)" \
		-kernel "$(WORKSTATION_LIVE_KERNEL)" \
		-initrd "$(WORKSTATION_LIVE_INITRD)" \
		-append "$(WORKSTATION_DEBUG_ARGS)" \
		-display gtk,gl=off \
		-serial stdio -monitor none -no-reboot
endif

workstation-diagnose-qemu:
ifeq ($(OS),Windows_NT)
	@if (-not (Test-Path '$(WORKSTATION_SEED_ISO)')) { Write-Error 'missing $(WORKSTATION_SEED_ISO); build it with make workstation-seed-iso-wsl first'; exit 1 }
	powershell -NoProfile -ExecutionPolicy Bypass -File tools/qemu_diagnostic_matrix.ps1
else
	@echo 'workstation-diagnose-qemu is currently implemented for the Windows QEMU host.'
endif

toolchain-check:
ifeq ($(OS),Windows_NT)
	@if (-not (Get-Command '$(CC)' -ErrorAction SilentlyContinue)) { Write-Error 'missing $(CC)'; exit 1 }
	@if (-not (Get-Command '$(LD)' -ErrorAction SilentlyContinue)) { Write-Error 'missing $(LD)'; exit 1 }
	@if (-not (Get-Command '$(OBJCOPY)' -ErrorAction SilentlyContinue)) { Write-Error 'missing $(OBJCOPY)'; exit 1 }
	@if (-not (Get-Command '$(PYTHON)' -ErrorAction SilentlyContinue)) { Write-Error 'missing $(PYTHON)'; exit 1 }
	@if (-not (Test-Path '$(XORRISO)')) { Write-Error 'missing $(XORRISO)'; exit 1 }
	@if (-not (Test-Path '$(LIMINE)')) { Write-Error 'missing $(LIMINE)'; exit 1 }
	@if (-not (Get-Command 'qemu-system-x86_64' -ErrorAction SilentlyContinue)) { Write-Error 'missing qemu-system-x86_64'; exit 1 }
	@if (-not (Test-Path '$(OVMF_CODE)')) { Write-Error 'missing $(OVMF_CODE)'; exit 1 }
	@if (-not (Test-Path '$(OVMF_VARS)')) { Write-Error 'missing $(OVMF_VARS)'; exit 1 }
	@if (-not (Test-Path '$(LIMINE_DIR)/limine-bios.sys')) { Write-Error 'missing $(LIMINE_DIR)/limine-bios.sys'; exit 1 }
	@if (-not (Test-Path '$(LIMINE_DIR)/limine-bios-cd.bin')) { Write-Error 'missing $(LIMINE_DIR)/limine-bios-cd.bin'; exit 1 }
	@if (-not (Test-Path '$(LIMINE_DIR)/limine-uefi-cd.bin')) { Write-Error 'missing $(LIMINE_DIR)/limine-uefi-cd.bin'; exit 1 }
	@if (-not (Test-Path '$(LIMINE_DIR)/BOOTX64.EFI')) { Write-Error 'missing $(LIMINE_DIR)/BOOTX64.EFI'; exit 1 }
else
	@command -v $(CC) >/dev/null || { echo "missing $(CC)"; exit 1; }
	@command -v $(OBJCOPY) >/dev/null || { echo "missing $(OBJCOPY)"; exit 1; }
	@command -v $(PYTHON) >/dev/null || { echo "missing $(PYTHON)"; exit 1; }
	@command -v $(XORRISO) >/dev/null || { echo "missing $(XORRISO)"; exit 1; }
	@command -v $(LIMINE) >/dev/null || { echo "missing $(LIMINE)"; exit 1; }
	@command -v qemu-system-x86_64 >/dev/null || { echo "missing qemu-system-x86_64"; exit 1; }
	@test -f "$(OVMF_CODE)" || { echo "missing $(OVMF_CODE)"; exit 1; }
	@test -f "$(OVMF_VARS)" || { echo "missing $(OVMF_VARS)"; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-bios.sys" || { echo "missing $(LIMINE_DIR)/limine-bios.sys"; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-bios-cd.bin" || { echo "missing $(LIMINE_DIR)/limine-bios-cd.bin"; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-uefi-cd.bin" || { echo "missing $(LIMINE_DIR)/limine-uefi-cd.bin"; exit 1; }
	@test -f "$(LIMINE_DIR)/BOOTX64.EFI" || { echo "missing $(LIMINE_DIR)/BOOTX64.EFI"; exit 1; }
endif

$(KERNEL): $(OBJECTS) kernel/linker.ld
	$(LINK) $(LDFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/kernel/fs/initramfs.o: $(USERLAND_INC)

$(USERLAND_INC): $(USER_ELFS) tools/package_userland.py
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(PYTHON) tools/package_userland.py --output $@ init=$(BUILD_DIR)/userland/init.elf session=$(BUILD_DIR)/userland/session.elf terminal=$(BUILD_DIR)/userland/terminal.elf settings=$(BUILD_DIR)/userland/settings.elf launcher=$(BUILD_DIR)/userland/launcher.elf sh=$(BUILD_DIR)/userland/sh.elf help=$(BUILD_DIR)/userland/help.elf sysinfo=$(BUILD_DIR)/userland/sysinfo.elf echo=$(BUILD_DIR)/userland/echo.elf

$(BUILD_DIR)/userland/%.elf: $(BUILD_DIR)/userland/%.o $(USER_COMMON_OBJECTS) userland/linker.ld
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(LINK) $(USER_LDFLAGS) -o $@ $< $(USER_COMMON_OBJECTS)

$(BUILD_DIR)/userland/%.bin: $(BUILD_DIR)/userland/%.elf
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/userland/%.o: userland/%.c
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(CC) $(USER_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/userland/%.o: userland/%.S
ifeq ($(OS),Windows_NT)
	@New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null
else
	@mkdir -p $(@D)
endif
	$(CC) $(USER_ASFLAGS) -MMD -MP -c $< -o $@

iso: toolchain-check $(KERNEL) boot/limine.conf
ifeq ($(OS),Windows_NT)
	@if (Test-Path '$(ISO_ROOT)') { Remove-Item -Recurse -Force '$(ISO_ROOT)' }
	@New-Item -ItemType Directory -Force -Path '$(ISO_ROOT)/boot', '$(ISO_ROOT)/EFI/BOOT' | Out-Null
	Copy-Item -Force '$(KERNEL)' '$(ISO_ROOT)/boot/zenithos.elf'
	Copy-Item -Force 'boot/limine.conf' '$(ISO_ROOT)/limine.conf'
	Copy-Item -Force '$(LIMINE_DIR)/limine-bios.sys' '$(ISO_ROOT)/'
	Copy-Item -Force '$(LIMINE_DIR)/limine-bios-cd.bin' '$(ISO_ROOT)/'
	Copy-Item -Force '$(LIMINE_DIR)/limine-uefi-cd.bin' '$(ISO_ROOT)/'
	Copy-Item -Force '$(LIMINE_DIR)/BOOTX64.EFI' '$(ISO_ROOT)/EFI/BOOT/'
	& '$(XORRISO)' -as mkisofs -R -r -J -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label $(ISO_ROOT) -o $(ISO)
	& '$(LIMINE)' bios-install $(ISO)
else
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/zenithos.elf
	cp boot/limine.conf $(ISO_ROOT)/limine.conf
	cp "$(LIMINE_DIR)/limine-bios.sys" $(ISO_ROOT)/
	cp "$(LIMINE_DIR)/limine-bios-cd.bin" $(ISO_ROOT)/
	cp "$(LIMINE_DIR)/limine-uefi-cd.bin" $(ISO_ROOT)/
	cp "$(LIMINE_DIR)/BOOTX64.EFI" $(ISO_ROOT)/EFI/BOOT/
	$(XORRISO) -as mkisofs -R -r -J \
		-b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $(ISO)
	$(LIMINE) bios-install $(ISO)
endif

run: iso
ifeq ($(OS),Windows_NT)
	Copy-Item -Force '$(OVMF_CODE)' '$(RUN_OVMF)'
	Copy-Item -Force '$(OVMF_VARS)' '$(RUN_OVMF_VARS)'
	qemu-system-x86_64 -machine q35 -m 1024M -cpu max -drive if=pflash,format=raw,readonly=on,file=$(RUN_OVMF) -drive if=pflash,format=raw,file=$(RUN_OVMF_VARS) -cdrom $(ISO) -boot d -serial file:$(BUILD_DIR)/serial.log -monitor none
else
	qemu-system-x86_64 -machine q35 -m 1024M -cpu max \
		-drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file="$(OVMF_VARS)" \
		-cdrom $(ISO) -boot d -serial file:$(BUILD_DIR)/serial.log -monitor none
endif

clean:
ifeq ($(OS),Windows_NT)
	@if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }
else
	rm -rf $(BUILD_DIR)
endif

-include $(OBJECTS:.o=.d)
