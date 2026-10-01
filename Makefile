# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2025

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
# -mgeneral-regs-only: without this, GCC is free to emit SSE/MMX/x87 FPU
# instructions (e.g. movdqu/movups) for things like struct-by-value copies -
# this freestanding kernel never initializes FPU/SSE state (no CR0/CR4 setup),
# so any such instruction immediately raises #UD. Confirmed via a real
# triple-fault caused by GCC vectorizing an EXT2DriverRequest struct copy.
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding -mgeneral-regs-only
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386


DISK_NAME = storage

run: all
	@qemu-system-i386 -s -S -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk
debug-run:
	@timeout 8 qemu-system-i386 -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -display none -serial file:$(OUTPUT_FOLDER)/out.log -no-reboot; true
all: build
build: iso
clean:
	rm -rf *.o *.iso $(OUTPUT_FOLDER)/kernel
disk:
	qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M



kernel:
	@$(ASM) $(AFLAGS) src/kernel-entrypoint.s -o bin/kernel-entrypoint.o
	@$(ASM) $(AFLAGS) src/cpu/interrupt.s -o bin/interrupt.o
	@$(CC) $(CFLAGS) src/kernel.c -o bin/kernel.o
	@$(CC) $(CFLAGS) src/cpu/gdt.c -o bin/gdt.o
	@$(CC) $(CFLAGS) src/cpu/portio.c -o bin/portio.o
	@$(CC) $(CFLAGS) src/cpu/idt.c -o bin/idt.o
	@$(CC) $(CFLAGS) src/driver/framebuffer.c -o bin/framebuffer.o
	@$(CC) $(CFLAGS) src/driver/keyboard.c -o bin/keyboard.o
	@$(CC) $(CFLAGS) src/driver/serial.c -o bin/serial.o
	@$(CC) $(CFLAGS) src/stdlib/string.c -o bin/string.o
	@$(CC) $(CFLAGS) src/driver/disk.c -o bin/disk.o
	@$(CC) $(CFLAGS) src/filesystem/ext2.c -o bin/ext2.o
	@$(CC) $(CFLAGS) src/memory/paging.c -o bin/paging.o
	@echo Linking object files and generate elf32...
	@$(LIN) $(LFLAGS) bin/kernel-entrypoint.o bin/interrupt.o bin/kernel.o bin/gdt.o bin/portio.o bin/idt.o bin/framebuffer.o bin/keyboard.o bin/serial.o bin/string.o bin/disk.o bin/ext2.o bin/paging.o -o $(OUTPUT_FOLDER)/kernel
	@rm -f bin/*.o

iso: kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1 $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst $(OUTPUT_FOLDER)/iso/boot/grub/
	@genisoimage -R \
		-b boot/grub/grub1 \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		-A os \
		-input-charset utf8 \
		-quiet \
		-o $(OUTPUT_FOLDER)/OS2025.iso \
		$(OUTPUT_FOLDER)/iso
	@rm -rf $(OUTPUT_FOLDER)/iso/
