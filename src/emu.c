#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "file.h"

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define arrsize(x) sizeof(x) / sizeof(x[0])

void print_reg(u32 *reg) {
    for (u32 i = 0; i < 32; i++) {
        printf("r%u: 0x%x\n", i, reg[i]);
    }
}

void syscall_exec(u32 *reg) {
    u32 syscall_num = reg[2];
    u32 arg_1 = reg[4]; // a0
    u32 arg_2 = reg[5]; // a1
    u32 arg_3 = reg[6]; // a2
    u32 arg_4 = reg[7]; // a3

    u32 sys_id = syscall_num - 0x4000;
    switch (sys_id) {
        case 1: {
            printf("Running exit\n");
            exit(arg_1);
        } break;
        case 4: {
            printf("Running write\n");
            char *msg = "MIPS HELLO\n";
            write(arg_1, msg, strlen(msg));
        } break;
        default: {
            printf("syscall 0x%x not supported!\n", syscall_num);
            print_reg(reg);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Invalid number of arguments!\n");
        return 1;
    }

    char *in_file = argv[1];

    File bin_file;
    if (read_file(in_file, &bin_file) == NULL) {
        return 1;
    }

    u32 *ops = (u32 *)bin_file.string;
    u32 num_ops = bin_file.size / 4;

    u32 reg[32] = {0};
    u32 pc = 0;

    for (; pc < num_ops; pc++) {
        u32 op = ops[pc];

        u8 op_id = op >> 26;
        u8 reg_1 = op << 6 >> 27;
        u8 reg_2 = op << 11 >> 27;
        u8 reg_3 = op << 16 >> 27;
        u8 sa = op << 21 >> 27;
        u8 special_op_id = op << 26 >> 26;
        u8 instr_idx = op << 6 >> 6;
        u16 imm = op << 16 >> 16;

        switch (op_id) {
            case 0: {
                switch (special_op_id) {
                    case 0x0: {
                        if (op == 0) {
                            printf("nop\n");
                        } else {
                            printf("sll r%u, r%u, %u\n", reg_3, reg_2, sa);
                            reg[reg_3] = reg[reg_2] << sa;
                        }
                    } break;
                    case 0x8: {
                        printf("jr r%u\n", reg_1);

                        pc = reg[reg_1];
                    } break;
                    case 0xc: {
                        printf("syscall\n");
                        syscall_exec(reg);
                    } break;
                    default: {
                        printf("Instruction %x not handled!\n", op);
                        printf("special op id: %u\n", special_op_id);
                        print_reg(reg);
                    }
                }
            } break;
            case 2: {
                printf("j 0x%x\n", instr_idx);
                pc = (pc >> 28 << 28) | (instr_idx - 1);
            } break;
            case 4: {
                u32 off = (imm + 1) << 2;
                printf("beq r%u, r%u, 0x%x\n", reg_1, reg_2, off);

                if (reg[reg_1] == reg[reg_2]) {
                    pc = pc + off;
                }
            } break;
            case 8: {
                printf("addi r%u, r%u, 0x%x\n", reg_1, reg_2, imm);

                reg[reg_2] = reg[reg_1] + imm;
            } break;
            case 9: {
                printf("addiu r%u, r%u, 0x%x\n", reg_1, reg_2, imm);

                reg[reg_2] = reg[reg_1] + imm;
            } break;
            case 13: {
                printf("ori r%u, r%u, 0x%x\n", reg_1, reg_2, imm);

                reg[reg_2] = reg[reg_1] | imm;
            } break;
            case 15: {
                u32 load_val = (imm << 16) & 0xFFFF0000;
                printf("lui r%u, 0x%x\n", reg_2, imm);

                reg[reg_2] = load_val;
            } break;
            default: {
                printf("Instruction %x not handled!\n", op);
                printf("op id: %u\n", op_id);
                print_reg(reg);
            }
        }
    }
}
