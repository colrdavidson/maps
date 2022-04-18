#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>

#define DEBUG 1

#include "common.h"
#include "map.h"
#include "file.h"
#include "elf.h"

typedef enum Op {
	Op_Add, Op_Addu,
	Op_Addi, Op_Addiu,
	Op_Mult, Op_Multu,
	Op_Sll, Op_Nop,
	Op_Ori, Op_Lui,
	Op_Jr, Op_J,
	Op_Jal, Op_Sub,
	Op_Bne, Op_Beq,
	Op_Lb, Op_Sb,
	Op_Lw, Op_Sw,
	Op_Data, Op_Syscall
} Op;

typedef enum Register {
	Reg_zero, Reg_at, Reg_v0, Reg_v1,
	Reg_a0, Reg_a1, Reg_a2, Reg_a3,
	Reg_t0, Reg_t1, Reg_t2, Reg_t3,
	Reg_t4, Reg_t5, Reg_t6, Reg_t7,
	Reg_s0, Reg_s1, Reg_s2, Reg_s3,
	Reg_s4, Reg_s5, Reg_s6, Reg_s7,
	Reg_t8, Reg_t9, Reg_k0, Reg_k1,
	Reg_gp, Reg_sp, Reg_fp, Reg_ra
} Register;

typedef enum Key {
	Key_Db, Key_Dh, Key_Dw, Key_Section
} Key;

typedef enum SectionType {
	Section_Text, Section_Data
} SectionType;

typedef struct Section {
	SectionType type;
	u32 start_idx;
	u32 end_idx;
} Section;

typedef struct Inst {
	Op op;
	Register reg[3];
	u32 imm;
	i16 rel_addr;
	u32 instr_idx;
	char *symbol_str;
	u8 width;
	u32 off;
	u8 *arr;
} Inst;

typedef struct Token {
	char *str;
	u32 size;
} Token;

typedef struct Symbol {
	u32 line_no;
	u32 inst_off;
} Symbol;

typedef struct InstArr {
	Inst *arr;
	u32 capacity;
	u32 size;
} InstArr;

InstArr iarr_init() {
	InstArr i;
	i.capacity = 10;
	i.size = 0;
	i.arr = (Inst *)malloc(sizeof(Inst) * i.capacity);
	return i;
}

void iarr_push(InstArr *i, Inst inst) {
	if (i->size >= i->capacity) {
		i->capacity *= 2;
		i->arr = realloc(i->arr, sizeof(Inst) * i->capacity);
	}

	i->arr[i->size] = inst;
	i->size++;
}

u32 line_no = 0;
Map *op_map;
Map *reg_map;
Map *keyword_map;
Map *section_map;
Map *label_map;
Map *symbol_map;

char *eat(char *str, int (*ptr)(int), int ret) {
	while (*str != '\0' && !!ptr(*str) == !!ret) {
		if (*str == '\n') {
			line_no++;
		}
		str++;
	}

	return str;
}

int has_close_bracket(int c) {
	if (c == ']') {
		return 0;
	}

	return 1;
}

int has_squote(int c) {
	if (c == '\'') {
		return 0;
	}

	return 1;
}

int has_dquote(int c) {
	if (c == '\"') {
		return 0;
	}

	return 1;
}

char *strip_str(char *str) {
	char *ptr = str;
	char *start_ptr = eat(ptr, isspace, 1);
	char *end_ptr = eat(start_ptr, isspace, 0);

	u32 str_size = end_ptr - start_ptr;
	char *stripped_str = calloc(1, str_size);
	memcpy(stripped_str, start_ptr, str_size);

	return stripped_str;
}

void get_token(char **ext_ptr, Token *tok) {
	char *ptr = *ext_ptr;
	char *start = ptr;
	ptr = eat(ptr, isspace, 0);

	tok->size = (u32)(ptr - start);
	memcpy(tok->str, start, tok->size);
	tok->str[tok->size] = 0;

	*ext_ptr = ptr;
}

bool parse_number(char *token, u32 *num) {
	int base = 10;
	char *strtol_start = token;
	if (token[0] == '0' && token[1] == 'x') {
		base = 16;
		strtol_start += 2;
	}

	char *endptr = NULL;
	errno = 0;
	u32 result = strtol(strtol_start, &endptr, base);
	if (strtol_start == endptr || (errno != 0 && result == 0)) {
		return false;
	}

	*num = result;

	return true;
}

inline void r_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 3;
	*expected_imm = 0;
	*expected_addr = 0;
}

inline void r2_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 2;
	*expected_imm = 0;
	*expected_addr = 0;
}

inline void r2_i_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 2;
	*expected_imm = 1;
	*expected_addr = 0;
}

inline void i_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 1;
	*expected_imm = 1;
	*expected_addr = 0;
}

inline void j_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 0;
	*expected_imm = 1;
	*expected_addr = 0;
}

inline void jr_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 1;
	*expected_imm = 0;
	*expected_addr = 0;
}

inline void d_args(u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	*expected_reg = 1;
	*expected_imm = 0;
	*expected_addr = 1;
}

void expected_args(Op op, u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
	switch (op) {
		case Op_Syscall: { *expected_reg = *expected_imm = *expected_addr = 0; } break;
		case Op_Nop: {     *expected_reg = *expected_imm = *expected_addr = 0; } break;
		case Op_Mult: {  r2_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Multu: { r2_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Add: {   r_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Sub: {   r_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Addu: {  r_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Addi: {  r2_i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Addiu: { r2_i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Ori: {   r2_i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Beq: {   r2_i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Bne: {   r2_i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Sll: {   r2_i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Lui: {   i_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_J: {     j_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Jr: {    jr_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Jal: {   j_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Lb: {    d_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Sb: {    d_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Sw: {    d_args(expected_reg, expected_imm, expected_addr); } break;
		case Op_Lw: {    d_args(expected_reg, expected_imm, expected_addr); } break;
		default: {
			printf("Unhandled op (Expected Args): %x\n", op);
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
usage:
		fprintf(stderr, "Usage: %s [-e] <in_file> <out_file>\n\t-e is for elf\n", argv[0]);
		return 1;
	}

	bool use_elf = false;

	int opt;
	while ((opt = getopt(argc, argv, "eh")) != -1) {
		switch (opt) {
			case 'e': {
				use_elf = true;
			} break;
			case 'h': {
				goto usage;
			} break;
			default: {
				goto usage;
			}
		}
	}

	char *in_file = argv[optind];
	char *out_file = argv[optind + 1];

	if (argv[optind + 2] != NULL) {
		goto usage;
	}

	File prog_file;
	if (read_file(in_file, &prog_file) == NULL) {
		return 1;
	}

	op_map = map_init();
	map_insert(op_map, "sub", (void *)Op_Sub);
	map_insert(op_map, "add", (void *)Op_Add);
	map_insert(op_map, "addu", (void *)Op_Addu);
	map_insert(op_map, "addi", (void *)Op_Addi);
	map_insert(op_map, "addiu", (void *)Op_Addiu);
	map_insert(op_map, "mult", (void *)Op_Mult);
	map_insert(op_map, "multu", (void *)Op_Multu);
	map_insert(op_map, "syscall", (void *)Op_Syscall);
	map_insert(op_map, "ori", (void *)Op_Ori);
	map_insert(op_map, "lui", (void *)Op_Lui);
	map_insert(op_map, "jr", (void *)Op_Jr);
	map_insert(op_map, "j", (void *)Op_J);
	map_insert(op_map, "jal", (void *)Op_Jal);
	map_insert(op_map, "sll", (void *)Op_Sll);
	map_insert(op_map, "nop", (void *)Op_Nop);
	map_insert(op_map, "bne", (void *)Op_Bne);
	map_insert(op_map, "beq", (void *)Op_Beq);
	map_insert(op_map, "lb", (void *)Op_Lb);
	map_insert(op_map, "lw", (void *)Op_Lw);
	map_insert(op_map, "sb", (void *)Op_Sb);
	map_insert(op_map, "sw", (void *)Op_Sw);

	reg_map = map_init();
	map_insert(reg_map, "zero", (void *)Reg_zero);
	map_insert(reg_map, "at", (void *)Reg_at);
	map_insert(reg_map, "v0", (void *)Reg_v0);
	map_insert(reg_map, "v1", (void *)Reg_v1);
	map_insert(reg_map, "a0", (void *)Reg_a0);
	map_insert(reg_map, "a1", (void *)Reg_a1);
	map_insert(reg_map, "a2", (void *)Reg_a2);
	map_insert(reg_map, "a3", (void *)Reg_a3);
	map_insert(reg_map, "t0", (void *)Reg_t0);
	map_insert(reg_map, "t1", (void *)Reg_t1);
	map_insert(reg_map, "t2", (void *)Reg_t2);
	map_insert(reg_map, "t3", (void *)Reg_t3);
	map_insert(reg_map, "t4", (void *)Reg_t4);
	map_insert(reg_map, "t5", (void *)Reg_t5);
	map_insert(reg_map, "t6", (void *)Reg_t6);
	map_insert(reg_map, "t7", (void *)Reg_t7);
	map_insert(reg_map, "s0", (void *)Reg_s0);
	map_insert(reg_map, "s1", (void *)Reg_s1);
	map_insert(reg_map, "s2", (void *)Reg_s2);
	map_insert(reg_map, "s3", (void *)Reg_s3);
	map_insert(reg_map, "s4", (void *)Reg_s4);
	map_insert(reg_map, "s5", (void *)Reg_s5);
	map_insert(reg_map, "s6", (void *)Reg_s6);
	map_insert(reg_map, "s7", (void *)Reg_s7);
	map_insert(reg_map, "t8", (void *)Reg_t8);
	map_insert(reg_map, "t9", (void *)Reg_t9);
	map_insert(reg_map, "k0", (void *)Reg_k0);
	map_insert(reg_map, "k1", (void *)Reg_k1);
	map_insert(reg_map, "gp", (void *)Reg_gp);
	map_insert(reg_map, "sp", (void *)Reg_sp);
	map_insert(reg_map, "fp", (void *)Reg_fp);
	map_insert(reg_map, "ra", (void *)Reg_ra);

	keyword_map = map_init();
	map_insert(keyword_map, "db", (void *)Key_Db);
	map_insert(keyword_map, "dh", (void *)Key_Dh);
	map_insert(keyword_map, "dw", (void *)Key_Dw);
	map_insert(keyword_map, "section", (void *)Key_Section);

	section_map = map_init();

	Section *text_sect = (Section *)calloc(1, sizeof(Section));
	text_sect->type = Section_Text;
	map_insert(section_map, "text", (void *)text_sect);

	Section *data_sect = (Section *)calloc(1, sizeof(Section));
	data_sect->type = Section_Data;
	map_insert(section_map, "data", (void *)data_sect);

	Section *cur_section = NULL;

	label_map = map_init();
	symbol_map = map_init();

	InstArr insts = iarr_init();

	char tok_buf[50];
	Token tok;
	tok.str = tok_buf;
	tok.size = 0;

	u32 inst_off = 0;

	char *program = prog_file.string;
	char *end_ptr = program + prog_file.size;
	char *ptr = program;
	while (ptr < end_ptr) {
		Inst inst;
		memset(&inst, 0, sizeof(Inst));
		inst.op = -1;

		ptr = eat(ptr, isspace, 1);

		if (ptr >= end_ptr) {
			// debug("Fell off the end, line: %llu\n", line_no);
			continue;
		}

		if (*ptr == ';') {
			while (*ptr != '\n' && *ptr != '\0' && ptr < end_ptr) {
				ptr++;
			}
			// debug("Skipped comment on line: %llu!\n", line_no);
			continue;
		}

		bzero(tok.str, tok.size);
		get_token(&ptr, &tok);

		if (tok.str[tok.size - 1] == ':') {
			char *label = (char *)calloc(tok.size - 1, sizeof(char));
			memcpy(label, tok.str, tok.size - 1);

			Symbol *s = (Symbol *)malloc(sizeof(Symbol));
			s->line_no = line_no;
			s->inst_off = inst_off;
			map_insert(label_map, label, (void *)s);

			debug("%s Label(%s): %u\n", tok.str, label, inst_off);

			continue;
		}

		Bucket key_bucket = map_get(keyword_map, tok.str);
		if (key_bucket.key != NULL) {
			Key key = (Key)key_bucket.data;
			debug("%s: Key(%u)\n", tok.str, key);

			ptr = eat(ptr, isspace, 1);
			bzero(tok.str, tok.size);

			if (key == Key_Section) {
				get_token(&ptr, &tok);
				Bucket section_bucket = map_get(section_map, tok.str);

				if (section_bucket.key != NULL) {
					Section *section = (Section *)section_bucket.data;

					section->start_idx = insts.size;
					if (cur_section != NULL && cur_section->type != section->type) {
						cur_section->end_idx = insts.size;
					}

					cur_section = section;

					debug("%s: Section(%d)\n", tok.str, section->type);
					continue;
				} else {
					printf("Invalid section %s\n", tok.str);
					return 1;
				}
			}

			if (ptr[0] == '\"') {
				ptr += 1;
				char *end_ptr = eat(ptr, has_dquote, 1);

				tok.size = (u32)(end_ptr - ptr);
				memcpy(tok.str, ptr, tok.size);
				tok.str[tok.size] = 0;

				ptr = end_ptr + 1;

				inst.op = Op_Data;
				inst.width = tok.size;

				u8 *inst_buf = calloc(1, tok.size);
				memcpy(inst_buf, tok.str, tok.size);

				inst.arr = inst_buf;
				inst.off = inst_off;
				inst_off += inst.width;

				iarr_push(&insts, inst);
				continue;
			}

			u32 result;
			if (ptr[0] == '\'') {
				ptr += 1;
				char *end_ptr = eat(ptr, has_squote, 1);

				tok.size = (u32)(end_ptr - ptr);
				memcpy(tok.str, ptr, tok.size);
				tok.str[tok.size] = 0;

				if (tok.size != 1) {
					printf("[%u] Invalid data %s\n", line_no + 1, tok.str);
					return 1;
				}

				result = tok.str[0];
				ptr = end_ptr + 1;
			} else {
				get_token(&ptr, &tok);

				if (!parse_number(tok.str, &result)) {
					printf("[%u] Invalid data %s\n", line_no + 1, tok.str);
					return 1;
				}
			}

			debug("%s: Data(%u)\n", tok.str, result);

			switch (key) {
				case Key_Db: {
					inst.op = Op_Data;
					inst.width = 1;
				} break;
				case Key_Dh: {
					inst.op = Op_Data;
					inst.width = 2;
				} break;
				case Key_Dw: {
					inst.op = Op_Data;
					inst.width = 4;
				} break;
				default: {}
			}

			inst.imm = result;
			inst.off = inst_off;
			inst_off += inst.width;

			iarr_push(&insts, inst);
			continue;
		}

		Bucket op_bucket = map_get(op_map, tok.str);
		if (op_bucket.key != NULL) {
			Op op;
			op = (Op)op_bucket.data;
			debug("%s: Op(%u)\n", tok.str, op);

			inst.op = op;
			inst.width = 4;

			u32 rem = inst_off % 4;
			if (rem) {
				inst_off += 4 - rem;
			}

			inst.off = inst_off;
			inst_off += inst.width;
		} else {
			printf("No valid Op found on line: %u!\n", line_no + 1);
			return 1;
		}

		u32 expected_reg = 0;
		u32 expected_imm = 0;
		u32 expected_addr = 0;
		expected_args(inst.op, &expected_reg, &expected_imm, &expected_addr);

		for (u32 i = 0; i < expected_reg; i++) {
			ptr = eat(ptr, isspace, 1);

			bzero(tok.str, tok.size);
			get_token(&ptr, &tok);

			Bucket reg_bucket = map_get(reg_map, tok.str);
			if (reg_bucket.key != NULL) {
				Register reg = (Register)reg_bucket.data;
				debug("%s: Register(%u)\n", tok.str, reg);

				inst.reg[i] = reg;
			} else {
				printf("[%u] Couldn't find register: %s\n", line_no + 1, tok.str);
				return 1;
			}
		}

		for (u32 i = 0; i < expected_imm; i++) {
			ptr = eat(ptr, isspace, 1);

			bzero(tok.str, tok.size);
			get_token(&ptr, &tok);

			u32 result = 0;

			if (!parse_number(tok.str, &result)) {
				debug("%s: Symbol(%s)\n", tok.str, tok.str);

				inst.symbol_str = (char *)calloc(tok.size, sizeof(char));
				strcpy(inst.symbol_str, tok.str);

				Symbol *s = (Symbol *)malloc(sizeof(Symbol));
				s->line_no = line_no;

				u32 rem = inst_off % 4;
				if (rem) {
					inst_off += 4 - rem;
				}

				s->inst_off = inst_off;

				map_insert(symbol_map, inst.symbol_str, (void *)s);
			} else {
				debug("%s: Imm(%u)\n", tok.str, result);
				inst.imm = result;
			}
		}

		if (expected_addr != 0) {
			ptr = eat(ptr, isspace, 1);
			if (*ptr != '[') {
				printf("Invalid address token!\n");
			}
			ptr++;

			char *end_ptr = eat(ptr, has_close_bracket, 1);

			bzero(tok.str, tok.size);
			memcpy(tok.str, ptr, end_ptr - ptr);
			tok.size = end_ptr - ptr;
			tok.str[tok.size] = '\0';
			ptr = end_ptr + 1;


			// TODO: This is kinda gross; Refactor?
			Token reg_tok = {0};
			Token off_tok = {0};
			for (char *c = tok.str; c < tok.str + tok.size; c++) {
				if (*c == '+') {
					u32 plus_idx = c - tok.str;

					char *tmp;

					reg_tok.size = plus_idx;
					reg_tok.str = (char *)calloc(1, reg_tok.size + 1);
					memcpy(reg_tok.str, tok.str, reg_tok.size);
					tmp = strip_str(reg_tok.str);
					free(reg_tok.str);
					reg_tok.str = tmp;

					off_tok.size = tok.size - plus_idx - 1;
					off_tok.str = (char *)calloc(1, off_tok.size + 1);
					memcpy(off_tok.str, tok.str + plus_idx + 1, off_tok.size);
					tmp = strip_str(off_tok.str);
					free(off_tok.str);
					off_tok.str = tmp;

					break;
				}
			}

			if (reg_tok.str != NULL) {
				bzero(tok.str, tok.size);
				tok.str = memcpy(tok.str, reg_tok.str, strlen(reg_tok.str));
				free(reg_tok.str);
			}

			Bucket reg_bucket = map_get(reg_map, tok.str);
			if (reg_bucket.key != NULL) {
				Register reg = (Register)reg_bucket.data;
				debug("%s: Register(%u)\n", tok.str, reg);

				inst.reg[1] = reg;
			} else {
				printf("[%u] Not enough registers for op!\n", line_no + 1);
				return 1;
			}

			if (off_tok.str != NULL) {
				u32 result;
				if (!parse_number(off_tok.str, &result)) {
					printf("[%u] Invalid offset %s\n", line_no + 1, off_tok.str);
					return 1;
				}

				debug("%s: Offset(%u)\n", off_tok.str, result);
				free(off_tok.str);

				inst.imm = result;
			}
		}

		if ((u32)inst.op == -1) {
			printf("[%u] Op fail\n", line_no + 1);
			return 1;
		}

		iarr_push(&insts, inst);
	}

	if (cur_section != NULL) {
		cur_section->end_idx = insts.size - 1;
	}

	u32 mem_start = 0x400000 + sizeof(Elf32_hdr) + sizeof(Program_hdr);
	for (u32 i = 0; i < symbol_map->capacity; i++) {
		if (symbol_map->m[i].key != NULL) {
			Symbol *sym_s = (Symbol *)symbol_map->m[i].data;
			Bucket label = map_get(label_map, symbol_map->m[i].key);
			if (label.key == NULL) {
				printf("Unable to resolve symbol %s!\n", symbol_map->m[i].key);
				return 1;
			}

			Symbol *lab_s = (Symbol *)label.data;

			u32 label_idx = (lab_s->inst_off >> 2) - 1;
			u32 symbol_idx = (sym_s->inst_off >> 2) - 1;

			insts.arr[symbol_idx].instr_idx = (mem_start + lab_s->inst_off);
			insts.arr[symbol_idx].rel_addr = (label_idx - symbol_idx);
			insts.arr[symbol_idx].imm = (mem_start + lab_s->inst_off);
			debug("Found symbol: %s, line: %u, offset: %u, idx: %x\n",
					symbol_map->m[i].key, sym_s->line_no,
					sym_s->inst_off, (insts.arr[symbol_idx].instr_idx)
				 );
		}
	}

	for (u32 i = 0; i < section_map->capacity; i++) {
		Bucket b = section_map->m[i];
		if (b.key != NULL) {
			Section *section = (Section *)b.data;

			u32 start_off = insts.arr[section->start_idx].off;
			u32 end_off = insts.arr[section->end_idx].off;
			u32 section_size = end_off - start_off;
			debug("section %s: %u bytes\n", b.key, section_size);
		}
	}

	Inst tmp_inst = insts.arr[insts.size - 1];
	u32 binary_size = tmp_inst.off + tmp_inst.width;
	u8 *binary = (u8 *)calloc(1, binary_size);

	u32 insert_idx = 0;
	for (u32 i = 0; i < insts.size; i++) {
		Inst inst = insts.arr[i];

		if (inst.op == Op_Data) {
			switch (inst.width) {
				case 1: {
					u8 inst_byte = inst.imm;
					memcpy(binary + insert_idx, &inst_byte, sizeof(inst_byte));
					debug("data: 0x%02x\n", inst_byte);
				} break;
				case 2: {
					u16 inst_bytes = inst.imm;
					memcpy(binary + insert_idx, &inst_bytes, sizeof(inst_bytes));
					debug("data: 0x%04x\n", inst_bytes);
				} break;
				case 4: {
					u32 inst_bytes = inst.imm;
					memcpy(binary + insert_idx, &inst_bytes, sizeof(inst_bytes));
					debug("data: 0x%08x\n", inst_bytes);
				} break;
				default: {
					u8 *inst_bytes = inst.arr;
					memcpy(binary + insert_idx, inst_bytes, inst.width);
					debug("data: %u bytes written\n", inst.width);
				}
			}

			insert_idx += inst.width;
			continue;
		}

		u32 inst_bytes = 0;
		switch (inst.op) {
			case Op_Syscall: { inst_bytes = 0xc; } break;
			case Op_Nop: {   inst_bytes = 0; } break;
			case Op_J: {     inst_bytes = 0x2 << 26 | inst.instr_idx; } break;
			case Op_Jal: {   inst_bytes = 0x3 << 26 | inst.instr_idx; } break;
			case Op_Jr: {    inst_bytes = inst.reg[0] << 21 | 0x8; } break;
			case Op_Mult: {  inst_bytes = inst.reg[0] << 21 | inst.reg[1] << 16 | 0x18; } break;
			case Op_Multu: { inst_bytes = inst.reg[0] << 21 | inst.reg[1] << 16 | 0x19; } break;
			case Op_Sll: {   inst_bytes = inst.reg[0] << 16 | inst.reg[1] << 11 | inst.reg[2] << 6; } break;
			case Op_Add: {   inst_bytes = inst.reg[2] << 21 | inst.reg[1] << 16 | inst.reg[0] << 11 | 0x20; } break;
			case Op_Addu: {  inst_bytes = inst.reg[2] << 21 | inst.reg[1] << 16 | inst.reg[0] << 11 | 0x21; } break;
			case Op_Sub: {   inst_bytes = inst.reg[2] << 21 | inst.reg[1] << 16 | inst.reg[0] << 11 | 0x22; } break;
			case Op_Lui: {   inst_bytes = 0xF  << 26 | inst.reg[0] << 16 | (u16)(inst.imm >> 16); } break;
			case Op_Beq: {   inst_bytes = 0x4  << 26 | inst.reg[0] << 21 | inst.reg[1] << 16 | (u16)inst.rel_addr; } break;
			case Op_Bne: {   inst_bytes = 0x5  << 26 | inst.reg[0] << 21 | inst.reg[1] << 16 | (u16)inst.rel_addr; } break;
			case Op_Ori: {   inst_bytes = 0xD  << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			case Op_Addi: {  inst_bytes = 0x8  << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			case Op_Addiu: { inst_bytes = 0x9  << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			case Op_Lb: {    inst_bytes = 0x20 << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			case Op_Lw: {    inst_bytes = 0x23 << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			case Op_Sb: {    inst_bytes = 0x28 << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			case Op_Sw: {    inst_bytes = 0x2B << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | (u16)inst.imm; } break;
			default: {
				printf("Unhandled op (Bin Generator): %x\n", inst.op);
				return 1;
			}
		}

		u32 rem = insert_idx % 4;
		if (rem != 0) {
			u8 pad_size = 4 - rem;
			insert_idx += pad_size;

			memset(binary + insert_idx, 0, pad_size);
		}

		debug("0x%08x\n", inst_bytes);

		u32 endian_inst_bytes = htonl(inst_bytes);
		//u32 endian_inst_bytes = inst_bytes;
		memcpy(binary + insert_idx, &endian_inst_bytes, sizeof(endian_inst_bytes));
		insert_idx += inst.width;
	}

	if (use_elf) {
		debug("Writing elf file!\n");

		write_elf_file(out_file, binary, insert_idx);
	} else {
		debug("Writing bin file!\n");

		FILE *binary_file = fopen(out_file, "wb");
		fwrite(binary, 1, insert_idx, binary_file);
		fclose(binary_file);
	}
}
