SHELL := /bin/bash
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

# Ubuntu x86_64/WSL2: GCC emits freestanding i386 objects, with no libc.
# A dedicated cross-toolchain can be used with: make CROSS=i686-elf-
CROSS ?=
CC := $(CROSS)gcc
LD := $(CROSS)ld
HOST_CC ?= gcc
PYTHON ?= python3
QEMU ?= qemu-system-i386
BUILD := build
KERNEL := $(BUILD)/alpacos.elf
ISO := $(BUILD)/alpacos.iso

CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -m32 -march=i686 -O2 -g -ffreestanding -fno-builtin \
          -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
          -fno-unwind-tables -mno-sse -mno-sse2 -mno-mmx -msoft-float \
          -Wall -Wextra -Werror -Wpedantic -MMD -MP
ASFLAGS := -m32 -ffreestanding -fno-pie -fno-pic -g -MMD -MP
LDFLAGS := -m elf_i386 -nostdlib --build-id=none -T boot/linker.ld \
           -Map=$(BUILD)/alpacos.map

C_SOURCES := kernel/main.c kernel/bootinfo.c kernel/console.c \
             kernel/shell/shell.c kernel/shell/parser.c \
             drivers/vga.c drivers/serial.c drivers/keyboard.c lib/kstring.c
ASM_SOURCES := boot/boot.S
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) \
           $(patsubst %.S,$(BUILD)/%.o,$(ASM_SOURCES))
QEMU_COMMON := -machine pc,accel=tcg -m 64M -smp 1 -nic none \
               -no-reboot -no-shutdown

.PHONY: all check test smoke smoke-iso run run-gui iso run-iso debug doctor clean help
all: $(KERNEL)

$(KERNEL): $(OBJECTS) boot/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

check: $(KERNEL)
	$(PYTHON) scripts/check_kernel.py $(KERNEL)

$(BUILD)/test_core: tests/test_core.c kernel/shell/parser.c lib/kstring.c \
                    include/alpacos/parser.h include/alpacos/kstring.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Wpedantic -fno-builtin \
		-Iinclude tests/test_core.c kernel/shell/parser.c lib/kstring.c -o $@

test: check $(BUILD)/test_core
	./$(BUILD)/test_core

smoke: check
	$(PYTHON) scripts/smoke_test.py --qemu "$(QEMU)" --kernel $(KERNEL)

run: $(KERNEL)
	@echo 'Salir de QEMU: Ctrl+A y luego X (sin mantener Ctrl).'
	$(QEMU) $(QEMU_COMMON) -kernel $(KERNEL) -display none -serial mon:stdio

run-gui: $(KERNEL)
	$(QEMU) $(QEMU_COMMON) -kernel $(KERNEL) -serial stdio -monitor none

iso: $(ISO)
$(ISO): $(KERNEL) boot/grub.cfg
	@command -v grub-mkrescue >/dev/null || { echo 'Falta grub-mkrescue: instala grub-common y grub-pc-bin.'; exit 1; }
	@command -v xorriso >/dev/null || { echo 'Falta xorriso.'; exit 1; }
	mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/alpacos.elf
	cp boot/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso

run-iso: $(ISO)
	$(QEMU) $(QEMU_COMMON) -cdrom $(ISO) -boot d -serial stdio -monitor none

smoke-iso: $(ISO)
	$(PYTHON) scripts/smoke_test.py --qemu "$(QEMU)" --iso $(ISO)

debug: $(KERNEL)
	@echo 'GDB local: target remote localhost:1234 (CPU pausada al iniciar).'
	$(QEMU) $(QEMU_COMMON) -kernel $(KERNEL) -display none \
		-serial mon:stdio -S -gdb tcp:127.0.0.1:1234

doctor:
	bash scripts/doctor.sh

clean:
	rm -rf $(BUILD)

help:
	@echo 'all       Compilar el kernel ELF de 32 bits'
	@echo 'check     Validar ELF y cabecera Multiboot'
	@echo 'test      Validacion estatica + pruebas de funciones puras'
	@echo 'smoke     Arrancar QEMU y comprobar el shell por puerto serie'
	@echo 'run       QEMU por terminal (funciona sin escritorio grafico)'
	@echo 'run-gui   QEMU con pantalla VGA y teclado PS/2'
	@echo 'iso       Crear ISO BIOS con GRUB (dependencias opcionales)'
	@echo 'run-iso   Arrancar la ISO en QEMU con ventana'
	@echo 'smoke-iso Comprobar automaticamente el arranque de la ISO'
	@echo 'debug     Arrancar pausado para GDB en localhost:1234'
	@echo 'doctor    Revisar herramientas instaladas'
	@echo 'clean     Borrar solamente build/'

-include $(OBJECTS:.o=.d)
