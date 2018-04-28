#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include "common.h"
#include "map.h"
#include "file.h"

typedef enum Op {
    Op_Addiu, Op_Syscall,
    Op_Sll, Op_Nop,
    Op_Ori, Op_Lui,
    Op_Jr, Op_J,
    Op_Bne, Op_Beq,
    Op_Lb, Op_Db,
    Op_Dw, Op_Dd,
    Op_Dq
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
    Key_Db, Key_Dw,
    Key_Dd, Key_Dq
} Key;

typedef struct Inst {
    Op op;
    Register reg[3];
    u16 imm;
    i16 rel_addr;
    u32 instr_idx;
    char *symbol_str;
    u8 width;
    u32 off;
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

char *eat(char *str, int (*ptr)(int), int ret) {
    while (*str != '\0' && ptr(*str) == ret) {
        if (*str == '\n') {
            line_no++;
        }
        str++;
    }

    return str;
}

Map *op_map;
Map *reg_map;
Map *keyword_map;
Map *label_map;
Map *symbol_map;

void get_token(char **ext_ptr, Token *tok) {
    char *ptr = *ext_ptr;
    char *start = ptr;
    ptr = eat(ptr, isspace, 0);

    tok->size = (u32)(ptr - start);
    memcpy(tok->str, start, tok->size);
    tok->str[tok->size] = 0;

    *ext_ptr = ptr;
}

void expected_args(Op op, u32 *expected_reg, u32 *expected_imm, u32 *expected_addr) {
    switch (op) {
        case Op_Addiu: {
            *expected_reg = 2;
            *expected_imm = 1;
        } break;
        case Op_Beq: {
            *expected_reg = 2;
            *expected_imm = 1;
        } break;
        case Op_Bne: {
            *expected_reg = 2;
            *expected_imm = 1;
        } break;
        case Op_Sll: {
            *expected_reg = 2;
            *expected_imm = 1;
        } break;
        case Op_Lui: {
            *expected_reg = 1;
            *expected_imm = 1;
        } break;
        case Op_Syscall: {
            *expected_reg = 0;
            *expected_imm = 0;
        } break;
        case Op_J: {
            *expected_reg = 0;
            *expected_imm = 1;
        } break;
        case Op_Nop: {
            *expected_reg = 0;
            *expected_imm = 0;
        } break;
        case Op_Lb: {
            *expected_reg = 1;
            *expected_imm = 0;
            *expected_addr = 1;
        } break;
        default: {
            printf("Unhandled op (Expected Args): %x\n", op);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Invalid number of arguments!\n");
        return 1;
    }

    char *in_file = argv[1];
    char *out_file = argv[2];

    File prog_file;
    if (read_file(in_file, &prog_file) == NULL) {
        return 1;
    }

    char *program = prog_file.string;
    FILE *binary_file = fopen(out_file, "w");

    op_map = map_init();
    map_insert(op_map, "addiu", (void *)Op_Addiu);
    map_insert(op_map, "syscall", (void *)Op_Syscall);
    map_insert(op_map, "ori", (void *)Op_Ori);
    map_insert(op_map, "lui", (void *)Op_Lui);
    map_insert(op_map, "jr", (void *)Op_Jr);
    map_insert(op_map, "j", (void *)Op_J);
    map_insert(op_map, "sll", (void *)Op_Sll);
    map_insert(op_map, "nop", (void *)Op_Nop);
    map_insert(op_map, "bne", (void *)Op_Bne);
    map_insert(op_map, "beq", (void *)Op_Beq);
    map_insert(op_map, "lb", (void *)Op_Lb);

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
    map_insert(keyword_map, "dw", (void *)Key_Dw);
    map_insert(keyword_map, "dd", (void *)Key_Dd);
    map_insert(keyword_map, "dq", (void *)Key_Dq);

    label_map = map_init();
    symbol_map = map_init();

    InstArr insts = iarr_init();

    char tok_buf[50];
    Token tok;
    tok.str = tok_buf;
    tok.size = 0;

    u32 inst_off = 0;

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
            get_token(&ptr, &tok);

            int base = 10;
            char *strtol_start = tok.str;
            if (tok.str[0] == '0' && tok.str[1] == 'x') {
                base = 16;
                strtol_start += 2;
            }

            char *endptr;
            errno = 0;
            u32 result = strtol(strtol_start, &endptr, base);
            if (errno != 0 && result == 0) {
                printf("Invalid data found on line: %u\n", line_no + 1);
                return 1;
            } else {
                debug("%s: Data(%u)\n", tok.str, result);

                switch (key) {
                    case Key_Db: {
                        inst.op = Op_Db;
                        inst.width = 1;
                    } break;
                    case Key_Dd: {
                        inst.op = Op_Dd;
                        inst.width = 2;
                    } break;
                    case Key_Dw: {
                        inst.op = Op_Dw;
                        inst.width = 4;
                    } break;
                    case Key_Dq: {
                        inst.op = Op_Dq;
                        inst.width = 8;
                    } break;
                }

                inst.imm = result;
                inst.off = inst_off;
                inst_off += inst.width;
            }

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
                inst_off += rem;
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
                printf("Not enough registers for op on line: %u\n", line_no + 1);
                return 1;
            }
        }

        for (u32 i = 0; i < expected_imm; i++) {
            ptr = eat(ptr, isspace, 1);

            bzero(tok.str, tok.size);
            get_token(&ptr, &tok);

            int base = 10;
            char *strtol_start = tok.str;
            if (tok.str[0] == '0' && tok.str[1] == 'x') {
                base = 16;
                strtol_start += 2;
            }

            char *endptr;
            errno = 0;
            u32 result = strtol(strtol_start, &endptr, base);
            if (errno != 0 && result == 0) {
                debug("%s: Symbol(%s)\n", tok.str, tok.str);

                inst.symbol_str = (char *)calloc(tok.size, sizeof(char));
                strcpy(inst.symbol_str, tok.str);

                Symbol *s = (Symbol *)malloc(sizeof(Symbol));
                s->line_no = line_no;
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

            bzero(tok.str, tok.size);
            get_token(&ptr, &tok);

            if (tok.str[tok.size - 1] == ']') {
                tok.size -= 1;
                tok.str[tok.size] = '\0';
            }

            Bucket reg_bucket = map_get(reg_map, tok.str);
            if (reg_bucket.key != NULL) {
                Register reg = (Register)reg_bucket.data;
                debug("%s: Register(%u)\n", tok.str, reg);

                inst.reg[1] = reg;
            } else {
                printf("Not enough registers for op on line: %u\n", line_no + 1);
                return 1;
            }
        }

        if ((u32)inst.op == -1) {
            printf("Op fail on line: %u\n", line_no + 1);
            return 1;
        }

        iarr_push(&insts, inst);
        if (*ptr == '\n') {
            line_no++;
        }
        ptr++;
    }

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

            insts.arr[symbol_idx].instr_idx = lab_s->inst_off;
            insts.arr[symbol_idx].rel_addr = (label_idx - symbol_idx);
            insts.arr[symbol_idx].imm = lab_s->inst_off;
            debug("Found symbol: %s, line: %u, offset: %u\n", symbol_map->m[i].key, sym_s->line_no, sym_s->inst_off);
        }
    }

    u32 insert_idx = 0;
    for (u32 i = 0; i < insts.size; i++) {
        Inst inst = insts.arr[i];

        switch (inst.op) {
            case Op_Db: {
                u8 inst_byte = inst.imm;
                debug("data: 0x%02x\n", inst_byte);
                fwrite(&inst_byte, sizeof(inst_byte), 1, binary_file);
                insert_idx += inst.width;
                continue;
            } break;
            case Op_Dw: {
                u16 inst_bytes = inst.imm;
                debug("data: 0x%04x\n", inst_bytes);
                fwrite(&inst_bytes, sizeof(inst_bytes), 1, binary_file);
                insert_idx += inst.width;
                continue;
            } break;
            case Op_Dd: {
                u32 inst_bytes = inst.imm;
                debug("data: 0x%08x\n", inst_bytes);
                fwrite(&inst_bytes, sizeof(inst_bytes), 1, binary_file);
                insert_idx += inst.width;
                continue;
            } break;
            case Op_Dq: {
                u64 inst_bytes = inst.imm;
                debug("data: 0x%016llx\n", inst_bytes);
                fwrite(&inst_bytes, sizeof(inst_bytes), 1, binary_file);
                insert_idx += inst.width;
                continue;
            } break;
            default: {}
        }

        u32 inst_bytes = 0;
        switch (inst.op) {
            case Op_Nop: {
                inst_bytes = 0;
            } break;
            case Op_Sll: {
                inst_bytes = inst.reg[0] << 16 | inst.reg[1] << 11 | inst.reg[2] << 6;
            } break;
            case Op_Beq: {
                inst_bytes = 0x4 << 26 | inst.reg[0] << 21 | inst.reg[1] << 16 | (u16)inst.rel_addr;
            } break;
            case Op_Bne: {
                inst_bytes = 0x5 << 26 | inst.reg[0] << 21 | inst.reg[1] << 16 | (u16)inst.rel_addr;
            } break;
            case Op_Syscall: {
                inst_bytes = 0xc;
            } break;
            case Op_J: {
                inst_bytes = 0x2 << 26 | inst.instr_idx;
            } break;
            case Op_Jr: {
                inst_bytes = inst.reg[0] << 21 | 0x8;
            } break;
            case Op_Ori: {
                inst_bytes = 0xD << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | inst.imm;
            } break;
            case Op_Lui: {
                inst_bytes = 0xF << 26 | inst.reg[0] << 16 | inst.imm;
            } break;
            case Op_Addiu: {
                inst_bytes = 0x9 << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | inst.imm;
            } break;
            case Op_Lb: {
                inst_bytes = 0x20 << 26 | inst.reg[1] << 21 | inst.reg[0] << 16 | inst.imm;
            } break;
            default: {
                printf("Unhandled op (Bin Generator): %x\n", inst.op);
                return 1;
            }
        }

        u32 rem = insert_idx % 4;
        if (rem != 0) {
            insert_idx += rem;

            u8 pad_byte = 0;
            debug("injecting %d pad bytes\n", rem);
            fwrite(&pad_byte, sizeof(pad_byte), rem, binary_file);
        }

        debug("0x%08x\n", inst_bytes);

        // u32 endian_inst_bytes = htonl(inst_bytes);
        u32 endian_inst_bytes = inst_bytes;
        fwrite(&endian_inst_bytes, sizeof(endian_inst_bytes), 1, binary_file);
    }
}
