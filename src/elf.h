#ifndef ELF_H
#define ELF_H

#include "common.h"

typedef struct {
    u32 magic;
    u8 bitness;
    u8 endian;
    u8 version_1;
    u8 os_abi;
    u64 pad;
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
#define ELF_VERSION 1
#define MIPS 0x8
#define EXECUTABLE 2
#define PT_LOAD 1

void write_elf_file(FILE *out_file, u8 *program, u32 program_size) {
    u32 file_size = sizeof(Elf32_hdr) + sizeof(Program_hdr) + program_size;

    u8 *out_bin = malloc(program_size);
    Elf32_hdr elf_hdr = {0};
    elf_hdr.magic = 'F' << 24 | 'L' << 16 | 'E' << 8 | 0x7F;
    elf_hdr.bitness = ELFCLASS32;
    elf_hdr.endian = ELFDATA2LSB;
    elf_hdr.version_1 = ELF_VERSION;
    elf_hdr.os_abi = 0;

    elf_hdr.type = EXECUTABLE;
    elf_hdr.machine = MIPS;
    elf_hdr.version_2 = ELF_VERSION;

    elf_hdr.program_entry = sizeof(Elf32_hdr) + sizeof(Program_hdr);

    elf_hdr.program_header_off = sizeof(Elf32_hdr);
    elf_hdr.section_header_off = 0;

    elf_hdr.flags = 0;
    elf_hdr.eh_size = sizeof(Elf32_hdr);

    elf_hdr.program_header_entry_size = sizeof(Program_hdr);
    elf_hdr.num_program_header_entries = 1;

    elf_hdr.section_header_entry_size = 0;
    elf_hdr.num_section_header_entries = 0;

    elf_hdr.section_header_names_idx = 0;

    Program_hdr prog_hdr = {0};
    prog_hdr.type = PT_LOAD;
    prog_hdr.off = elf_hdr.program_entry;
    prog_hdr.vaddr = 0;
    prog_hdr.paddr = 0;
    prog_hdr.file_size = file_size;
    prog_hdr.mem_size = file_size;
    prog_hdr.flags = 5;
    prog_hdr.align = 0x1000;

    memcpy(out_bin, &elf_hdr, sizeof(elf_hdr));
    memcpy(out_bin + sizeof(elf_hdr), &prog_hdr, sizeof(prog_hdr));
    memcpy(out_bin + sizeof(elf_hdr) + sizeof(prog_hdr), program, program_size);

    fwrite(out_bin, 1, file_size, out_file);
}

#endif
