#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "file.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

typedef enum Op {
    Op_Addiu,
    Op_Syscall,
    Op_Sll,
    Op_Nop,
    Op_Ori,
    Op_Lui,
    Op_Jr,
    Op_J
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

typedef struct Bucket {
    char *key;
    void *data;
} Bucket;

typedef struct Map {
    u32 size;
    u32 capacity;
    Bucket *m;
} Map;

Map *map_init() {
    Map *map = (Map *)malloc(sizeof(Map));
    map->size = 0;
    map->capacity = 20;
    map->m = (Bucket *)calloc(sizeof(Bucket), map->capacity);

    return map;
}

u32 map_hash(const char *key) {
    u32 hash = 0;
    for (u32 i = 0; i < strlen(key); i++) {
        hash = hash ^ key[i];
    }

    return hash;
}

void map_print(Map *map) {
    for (u32 i = 0; i < map->capacity; i++) {
        Bucket b = map->m[i];
        if (b.key == NULL) {
            printf("empty\n");
        } else {
            printf("key: %s\n", b.key);
        }
    }
}

void map_insert(Map *map, char *key, void *data) {
    u32 hash = map_hash(key) % map->capacity;

    for (u32 i = 0; i < map->capacity; i++) {
        Bucket b = map->m[hash];
        if (b.key == NULL) {
            map->m[hash].key = key;
            map->m[hash].data = data;
            map->size += 1;
            return;
        } else {
            hash = (hash + 1) % map->capacity;
        }
    }

    printf("Failed to insert into map!\n");
}

Bucket map_get(Map *map, char *key) {
    u32 hash = map_hash(key) % map->capacity;

    Bucket b;
    for (u32 i = 0; i < map->capacity; i++) {
        b = map->m[hash];
        if (b.key != NULL) {
            if (strcmp(b.key, key) == 0) {
                return map->m[hash];
            }
        }
        hash = (hash + 1) % map->capacity;
    }

    printf("Failed to get %s\n", key);
    b.key = NULL;
    return b;
}

typedef struct Inst {
    Op op;
    Register reg[3];
    u16 imm;
    u32 instr_idx;
    char *symbol_str;
} Inst;

typedef struct Token {
    char *str;
    u32 size;
} Token;

typedef struct Symbol {
    u64 line_no;
    u64 inst_no;
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

void eat_space(char **ptr) {
    while (**ptr == ' ') {
        *ptr += 1;
    }
}

void get_token(char **ext_ptr, Token *tok) {
    char *ptr = *ext_ptr;
    char *start = ptr;
    while (*ptr != ' ' && *ptr != '\0' && *ptr != '\n') {
        ptr++;
    }

    tok->size = (u32)(ptr - start);
    memcpy(tok->str, start, tok->size);
    tok->str[tok->size] = 0;

    *ext_ptr = ptr;
}

void expected_args(Op op, u32 *expected_reg, u32 *expected_imm) {
    switch (op) {
        case Op_Addiu: {
            *expected_reg = 2;
            *expected_imm = 1;
        } break;
        case Op_Sll: {
            *expected_reg = 2;
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

    Map *op_map = map_init();
    map_insert(op_map, "addiu", (void *)Op_Addiu);
    map_insert(op_map, "syscall", (void *)Op_Syscall);
    map_insert(op_map, "ori", (void *)Op_Ori);
    map_insert(op_map, "lui", (void *)Op_Lui);
    map_insert(op_map, "jr", (void *)Op_Jr);
    map_insert(op_map, "j", (void *)Op_J);
    map_insert(op_map, "sll", (void *)Op_Sll);
    map_insert(op_map, "nop", (void *)Op_Nop);

    Map *reg_map = map_init();
    map_insert(reg_map, "zero", (void *)Reg_zero);
    map_insert(reg_map, "a0", (void *)Reg_a0);
    map_insert(reg_map, "a1", (void *)Reg_a1);
    map_insert(reg_map, "a2", (void *)Reg_a2);
    map_insert(reg_map, "v0", (void *)Reg_v0);

    Map *label_map = map_init();
    Map *symbol_map = map_init();

    InstArr insts = iarr_init();

    char tok_buf[50];
    Token tok;
    tok.str = tok_buf;
    tok.size = 0;

    u64 line_no = 0;
    char *end_ptr = program + prog_file.size;
    char *ptr = program;
    while (ptr < end_ptr) {
        Inst inst;
        memset(&inst, 0, sizeof(Inst));
        inst.op = -1;

        while (*ptr == ' ' || *ptr == '\n') {
            eat_space(&ptr);
            if (*ptr == '\n') {
                line_no++;
                ptr++;
            }
        }

        if (*ptr == ';') {
            while (ptr < end_ptr && *ptr != '\n' && *ptr != '\0') {
                ptr++;
            }
            debug("Skipped comment on line: %llu!\n", line_no);
            continue;
        }

        bzero(tok.str, tok.size);
        get_token(&ptr, &tok);

        if (tok.str[tok.size - 1] == ':') {
            char *label = (char *)calloc(sizeof(char), tok.size - 1);
            memcpy(label, tok.str, tok.size - 1);

            Symbol *s = (Symbol *)malloc(sizeof(Symbol));
            s->line_no = line_no;
            s->inst_no = insts.size;
            map_insert(label_map, label, (void *)s);

            debug("%s Label(%s): %llu\n", tok.str, label, line_no + 1);

            continue;
        }

        Bucket op_bucket = map_get(op_map, tok.str);
        if (op_bucket.key != NULL) {
            Op op;
            op = (Op)op_bucket.data;
            debug("%s: Op(%u)\n", tok.str, op);

            inst.op = op;
        } else {
            printf("No valid Op found on line: %llu!\n", line_no + 1);
            return 1;
        }

        u32 expected_reg = 0;
        u32 expected_imm = 0;
        expected_args(inst.op, &expected_reg, &expected_imm);

        for (u32 i = 0; i < expected_reg; i++) {
            eat_space(&ptr);

            bzero(tok.str, tok.size);
            get_token(&ptr, &tok);

            Bucket reg_bucket = map_get(reg_map, tok.str);
            if (reg_bucket.key != NULL) {
                Register reg = (Register)reg_bucket.data;
                debug("%s: Register(%u)\n", tok.str, reg);

                inst.reg[i] = reg;
            } else {
                printf("Not enough registers for op on line: %llu\n", line_no + 1);
                return 1;
            }
        }

        for (u32 i = 0; i < expected_imm; i++) {
            eat_space(&ptr);

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

                inst.symbol_str = (char *)calloc(sizeof(char), tok.size);
                strcpy(inst.symbol_str, tok.str);

                Symbol *s = (Symbol *)malloc(sizeof(Symbol));
                s->line_no = line_no;
                s->inst_no = insts.size;

                map_insert(symbol_map, inst.symbol_str, (void *)s);
            } else {
                debug("%s: Imm(%u)\n", tok.str, result);
                inst.imm = result;
            }
        }

        if ((u32)inst.op == -1) {
            printf("Op fail on line: %llu\n", line_no + 1);
            return 1;
        }

        iarr_push(&insts, inst);
        line_no++;
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
            insts.arr[sym_s->inst_no].instr_idx = lab_s->inst_no;
            debug("Found symbol: %s, line: %llu, inst: %llu\n", symbol_map->m[i].key, sym_s->line_no, sym_s->inst_no);
        }
    }

    for (u32 i = 0; i < insts.size; i++) {
        Inst inst = insts.arr[i];

        u32 inst_bytes = 0;
        switch (inst.op) {
            case Op_Nop: {
                inst_bytes = 0;
            } break;
            case Op_Sll: {
                inst_bytes = inst.reg[0] << 16 | inst.reg[1] << 11 | inst.reg[2] << 6;
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
            default: {
                printf("Unhandled op (Bin Generator): %x\n", inst.op);
                return 1;
            }
        }

        debug("0x%08x\n", inst_bytes);

        // u32 endian_inst_bytes = htonl(inst_bytes);
        u32 endian_inst_bytes = inst_bytes;
        fwrite(&endian_inst_bytes, sizeof(u32), 1, binary_file);
    }
}
