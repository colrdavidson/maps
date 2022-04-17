#ifndef ELF_H
#define ELF_H

#include <arpa/inet.h>
#include "common.h"

typedef struct {
	u32 magic;
	u8 bitness;
	u8 endian;
	u8 version_1;
	u8 os_abi;
	u32 pad_1;
	u16 pad_2;
	u8 pad_3;
	u16 type;
	u16 machine;
	u32 version_2;
	u32 program_entry;
	u32 program_header_off;
	u32 section_header_off;
	u32 flags;
	u16 eh_size;
	u16 program_header_entry_size;
	u16 num_program_header_entries;
	u16 section_header_entry_size;
	u16 num_section_header_entries;
	u16 section_header_names_idx;
} Elf32_hdr;

typedef struct {
	u32 type;
	u32 off;
	u32 vaddr;
	u32 paddr;
	u32 file_size;
	u32 mem_size;
	u32 flags;
	u32 align;
} Program_hdr;

#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2
#define ELF_VERSION 1
#define MIPS 0x8
#define EXECUTABLE 2
#define PT_LOAD 1

#define EF_MIPS_NOREORDER 0x00000001
#define EF_MIPS_CPIC      0x00000004
#define EF_O32            0x00001000
#define EF_MIPS_ARCH_1    0x10000000

void write_elf_file(FILE *out_file, u8 *program, u32 program_size) {
	u32 file_size = sizeof(Elf32_hdr) + sizeof(Program_hdr) + program_size;

	u32 mem_location = 0x400000;

	u32 program_entrypoint = sizeof(Elf32_hdr) + sizeof(Program_hdr) + mem_location;

	u8 *out_bin = calloc(1, file_size);
	Elf32_hdr elf_hdr = {0};
	elf_hdr.magic = 'F' << 24 | 'L' << 16 | 'E' << 8 | 0x7F;
	elf_hdr.bitness = ELFCLASS32;
	elf_hdr.endian = ELFDATA2MSB;
	elf_hdr.version_1 = ELF_VERSION;
	elf_hdr.os_abi = 0;

	elf_hdr.type = htons(EXECUTABLE);
	elf_hdr.machine = htons(MIPS);
	elf_hdr.version_2 = htonl(ELF_VERSION);

	elf_hdr.program_entry = htonl(program_entrypoint);

	elf_hdr.program_header_off = htonl(sizeof(Elf32_hdr));
	elf_hdr.section_header_off = 0;

	elf_hdr.flags = htonl(EF_MIPS_NOREORDER | EF_O32 | EF_MIPS_CPIC | EF_MIPS_ARCH_1);
	elf_hdr.eh_size = htons(sizeof(Elf32_hdr));

	elf_hdr.program_header_entry_size = htons(sizeof(Program_hdr));
	elf_hdr.num_program_header_entries = htons(1);

	elf_hdr.section_header_entry_size = 0;
	elf_hdr.num_section_header_entries = 0;

	elf_hdr.section_header_names_idx = 0;

	Program_hdr prog_hdr = {0};
	prog_hdr.type = htonl(PT_LOAD);
	prog_hdr.off = htonl(0);
	prog_hdr.vaddr = htonl(mem_location);
	prog_hdr.paddr = prog_hdr.vaddr;
	prog_hdr.file_size = htonl(file_size);
	prog_hdr.mem_size = htonl(file_size);
	prog_hdr.flags = htonl(5);
	prog_hdr.align = htonl(0x1000);


	memcpy(out_bin, &elf_hdr, sizeof(elf_hdr));
	memcpy(out_bin + sizeof(elf_hdr), &prog_hdr, sizeof(prog_hdr));
	memcpy(out_bin + sizeof(elf_hdr) + sizeof(prog_hdr), program, program_size);

	fwrite(out_bin, 1, file_size, out_file);
}

#endif
