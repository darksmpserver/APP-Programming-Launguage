#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define STACK_SIZE 1024
#define MEM_SIZE 256
#define MAX_PROGRAM_SIZE 8192
#define TOKEN_SIZE 128
#define MAX_LABELS 256

typedef enum {
    OP_PUSH,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_XOR,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_SHL,
    OP_SHR,
    OP_DUP,
    OP_SWAP,
    OP_DROP,
    OP_OVER,
    OP_ROT,
    OP_NIP,
    OP_TUCK,
    OP_STORE,
    OP_LOAD,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,
    OP_JMP,
    OP_JZ,
    OP_JNZ,
    OP_CALL,
    OP_RET,
    OP_LABEL,
    OP_PRINT,
    OP_PRINTS,
    OP_INPUT,
    OP_INC,
    OP_DEC,
    OP_ABS,
    OP_NEG,
    OP_MIN,
    OP_MAX,
    OP_POW,
    OP_SQRT,
    OP_HALT,
    OP_NOP,
    OP_EOF
} AppCmdType;

typedef struct {
    AppCmdType type;
    long long val;
    char label_name[64];
} Instruction;

typedef struct {
    char name[64];
    int target_index;
} LabelMap;

Instruction program[MAX_PROGRAM_SIZE];
int prog_len = 0;

LabelMap labels[MAX_LABELS];
int label_count = 0;

long long stack[STACK_SIZE];
long long call_stack[STACK_SIZE];
long long registers[MEM_SIZE];
int stack_top = -1;
int call_stack_top = -1;

void panic(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

void stack_push(long long val) {
    if (stack_top >= STACK_SIZE - 1) {
        panic("Stack overflow");
    }
    stack[++stack_top] = val;
}

long long stack_pop(void) {
    if (stack_top < 0) {
        panic("Stack underflow");
    }
    return stack[stack_top--];
}

long long stack_peek(void) {
    if (stack_top < 0) {
        panic("Stack empty");
    }
    return stack[stack_top];
}

void call_stack_push(long long val) {
    if (call_stack_top >= STACK_SIZE - 1) {
        panic("Call stack overflow");
    }
    call_stack[++call_stack_top] = val;
}

long long call_stack_pop(void) {
    if (call_stack_top < 0) {
        panic("Call stack underflow");
    }
    return call_stack[call_stack_top--];
}

void add_label(const char *name, int index) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            labels[i].target_index = index;
            return;
        }
    }
    if (label_count >= MAX_LABELS) {
        panic("Too many labels");
    }
    strncpy(labels[label_count].name, name, 63);
    labels[label_count].name[63] = '\0';
    labels[label_count].target_index = index;
    label_count++;
}

int find_label(const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return labels[i].target_index;
        }
    }
    return -1;
}

void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

int is_delimiter(char c) {
    return isspace((unsigned char)c) || c < 32 || c == ';';
}

void parse_source_code(const char *source);

void include_library(const char *lib_filename) {
    FILE *f = fopen(lib_filename, "r");
    if (!f) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "libs/%s", lib_filename);
        f = fopen(full_path, "r");
        if (!f) {
            char err_buf[512];
            snprintf(err_buf, sizeof(err_buf), "Could not open include file '%s'", lib_filename);
            panic(err_buf);
        }
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return;
    }

    char *lib_source = (char *)malloc(fsize + 1);
    if (!lib_source) {
        fclose(f);
        panic("Memory allocation failed for library");
    }

    size_t len = fread(lib_source, 1, fsize, f);
    lib_source[len] = '\0';
    fclose(f);

    parse_source_code(lib_source);
    free(lib_source);
}

void parse_source_code(const char *source) {
    char token[TOKEN_SIZE];
    int pos = 0;

    if ((unsigned char)source[0] == 0xEF && (unsigned char)source[1] == 0xBB && (unsigned char)source[2] == 0xBF) {
        pos += 3;
    }

    while (source[pos] != '\0') {
        while (source[pos] != '\0') {
            if (source[pos] == '-' && source[pos + 1] == '-') {
                while (source[pos] != '\0' && source[pos] != '\n' && source[pos] != '\r') {
                    pos++;
                }
                continue;
            }
            if (is_delimiter(source[pos])) {
                pos++;
            } else {
                break;
            }
        }

        if (source[pos] == '\0') break;

        if (strncmp(&source[pos], "#include", 8) == 0 && (isspace((unsigned char)source[pos + 8]) || source[pos + 8] == '"')) {
            pos += 8;
            while (source[pos] != '\0' && source[pos] != '"' && source[pos] != '\n' && source[pos] != '\r') {
                pos++;
            }
            if (source[pos] == '"') {
                pos++;
                int path_len = 0;
                char lib_path[256];
                while (source[pos] != '\0' && source[pos] != '"' && source[pos] != '\n' && source[pos] != '\r') {
                    if (path_len < 255) lib_path[path_len++] = source[pos];
                    pos++;
                }
                lib_path[path_len] = '\0';
                if (source[pos] == '"') pos++;
                include_library(lib_path);
                continue;
            }
        }

        int t_len = 0;
        while (source[pos] != '\0' && !is_delimiter(source[pos])) {
            if (source[pos] == '-' && source[pos + 1] == '-') {
                break;
            }
            if (t_len < TOKEN_SIZE - 1) token[t_len++] = source[pos];
            pos++;
        }
        token[t_len] = '\0';

        if (token[0] == '\0') continue;

        if (token[t_len - 1] == ':') {
            token[t_len - 1] = '\0';
            to_lowercase(token);
            add_label(token, prog_len);
            program[prog_len].type = OP_LABEL;
            strncpy(program[prog_len].label_name, token, 63);
            prog_len++;
            continue;
        }

        char lower_token[TOKEN_SIZE];
        strncpy(lower_token, token, TOKEN_SIZE - 1);
        lower_token[TOKEN_SIZE - 1] = '\0';
        to_lowercase(lower_token);

        if (isdigit((unsigned char)token[0]) || (token[0] == '-' && isdigit((unsigned char)token[1]))) {
            program[prog_len].type = OP_PUSH;
            program[prog_len].val = atoll(token);
        } else if (strcmp(lower_token, "add") == 0) {
            program[prog_len].type = OP_ADD;
        } else if (strcmp(lower_token, "sub") == 0) {
            program[prog_len].type = OP_SUB;
        } else if (strcmp(lower_token, "mul") == 0) {
            program[prog_len].type = OP_MUL;
        } else if (strcmp(lower_token, "div") == 0) {
            program[prog_len].type = OP_DIV;
        } else if (strcmp(lower_token, "mod") == 0) {
            program[prog_len].type = OP_MOD;
        } else if (strcmp(lower_token, "xor") == 0) {
            program[prog_len].type = OP_XOR;
        } else if (strcmp(lower_token, "and") == 0) {
            program[prog_len].type = OP_AND;
        } else if (strcmp(lower_token, "or") == 0) {
            program[prog_len].type = OP_OR;
        } else if (strcmp(lower_token, "not") == 0) {
            program[prog_len].type = OP_NOT;
        } else if (strcmp(lower_token, "shl") == 0) {
            program[prog_len].type = OP_SHL;
        } else if (strcmp(lower_token, "shr") == 0) {
            program[prog_len].type = OP_SHR;
        } else if (strcmp(lower_token, "dup") == 0) {
            program[prog_len].type = OP_DUP;
        } else if (strcmp(lower_token, "swap") == 0) {
            program[prog_len].type = OP_SWAP;
        } else if (strcmp(lower_token, "drop") == 0) {
            program[prog_len].type = OP_DROP;
        } else if (strcmp(lower_token, "over") == 0) {
            program[prog_len].type = OP_OVER;
        } else if (strcmp(lower_token, "rot") == 0) {
            program[prog_len].type = OP_ROT;
        } else if (strcmp(lower_token, "nip") == 0) {
            program[prog_len].type = OP_NIP;
        } else if (strcmp(lower_token, "tuck") == 0) {
            program[prog_len].type = OP_TUCK;
        } else if (strcmp(lower_token, "eq") == 0) {
            program[prog_len].type = OP_EQ;
        } else if (strcmp(lower_token, "neq") == 0) {
            program[prog_len].type = OP_NEQ;
        } else if (strcmp(lower_token, "lt") == 0) {
            program[prog_len].type = OP_LT;
        } else if (strcmp(lower_token, "gt") == 0) {
            program[prog_len].type = OP_GT;
        } else if (strcmp(lower_token, "lte") == 0) {
            program[prog_len].type = OP_LTE;
        } else if (strcmp(lower_token, "gte") == 0) {
            program[prog_len].type = OP_GTE;
        } else if (strcmp(lower_token, "inc") == 0) {
            program[prog_len].type = OP_INC;
        } else if (strcmp(lower_token, "dec") == 0) {
            program[prog_len].type = OP_DEC;
        } else if (strcmp(lower_token, "abs") == 0) {
            program[prog_len].type = OP_ABS;
        } else if (strcmp(lower_token, "neg") == 0) {
            program[prog_len].type = OP_NEG;
        } else if (strcmp(lower_token, "min") == 0) {
            program[prog_len].type = OP_MIN;
        } else if (strcmp(lower_token, "max") == 0) {
            program[prog_len].type = OP_MAX;
        } else if (strcmp(lower_token, "pow") == 0) {
            program[prog_len].type = OP_POW;
        } else if (strcmp(lower_token, "sqrt") == 0) {
            program[prog_len].type = OP_SQRT;
        } else if (strcmp(lower_token, "print") == 0) {
            program[prog_len].type = OP_PRINT;
        } else if (strcmp(lower_token, "prints") == 0) {
            program[prog_len].type = OP_PRINTS;
        } else if (strcmp(lower_token, "input") == 0) {
            program[prog_len].type = OP_INPUT;
        } else if (strcmp(lower_token, "halt") == 0) {
            program[prog_len].type = OP_HALT;
        } else if (strcmp(lower_token, "nop") == 0) {
            program[prog_len].type = OP_NOP;
        } else if (strcmp(lower_token, "ret") == 0) {
            program[prog_len].type = OP_RET;
        } else if (strncmp(lower_token, "jmp", 3) == 0 || strncmp(lower_token, "jz", 2) == 0 || strncmp(lower_token, "jnz", 3) == 0 || strncmp(lower_token, "call", 4) == 0) {
            if (strncmp(lower_token, "jmp", 3) == 0) {
                program[prog_len].type = OP_JMP;
            } else if (strncmp(lower_token, "jz", 2) == 0) {
                program[prog_len].type = OP_JZ;
            } else if (strncmp(lower_token, "jnz", 3) == 0) {
                program[prog_len].type = OP_JNZ;
            } else {
                program[prog_len].type = OP_CALL;
            }

            int lbl_pos = pos;
            while (source[lbl_pos] != '\0' && (is_delimiter(source[lbl_pos]) || (source[lbl_pos] == '-' && source[lbl_pos + 1] == '-'))) {
                if (source[lbl_pos] == '-' && source[lbl_pos + 1] == '-') {
                    while (source[lbl_pos] != '\0' && source[lbl_pos] != '\n' && source[lbl_pos] != '\r') {
                        lbl_pos++;
                    }
                } else {
                    lbl_pos++;
                }
            }
            int l_len = 0;
            char target_lbl[64];
            while (source[lbl_pos] != '\0' && !is_delimiter(source[lbl_pos])) {
                if (l_len < 63) target_lbl[l_len++] = source[lbl_pos];
                lbl_pos++;
            }
            target_lbl[l_len] = '\0';
            to_lowercase(target_lbl);
            strncpy(program[prog_len].label_name, target_lbl, 63);
            pos = lbl_pos;
        } else if (lower_token[0] == 's' && lower_token[1] == 'r') {
            program[prog_len].type = OP_STORE;
            program[prog_len].val = atoi(&lower_token[2]);
        } else if (lower_token[0] == 'l' && lower_token[1] == 'r') {
            program[prog_len].type = OP_LOAD;
            program[prog_len].val = atoi(&lower_token[2]);
        } else {
            char err_buf[128];
            snprintf(err_buf, sizeof(err_buf), "Unknown instruction '%s'", token);
            panic(err_buf);
        }

        prog_len++;
        if (prog_len >= MAX_PROGRAM_SIZE) {
            panic("Program too large");
        }
    }
}

void parse_esoteric_app(const char *source) {
    prog_len = 0;
    label_count = 0;
    parse_source_code(source);
    program[prog_len].type = OP_EOF;
}

void execute_program(void) {
    int pc = 0;
    while (pc < prog_len) {
        Instruction inst = program[pc];

        switch (inst.type) {
            case OP_PUSH:
                stack_push(inst.val);
                break;
            case OP_ADD: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a + b);
                break;
            }
            case OP_SUB: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a - b);
                break;
            }
            case OP_MUL: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a * b);
                break;
            }
            case OP_DIV: {
                long long b = stack_pop();
                long long a = stack_pop();
                if (b == 0) panic("Division by zero");
                stack_push(a / b);
                break;
            }
            case OP_MOD: {
                long long b = stack_pop();
                long long a = stack_pop();
                if (b == 0) panic("Modulo by zero");
                stack_push(a % b);
                break;
            }
            case OP_XOR: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a ^ b);
                break;
            }
            case OP_AND: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a & b);
                break;
            }
            case OP_OR: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a | b);
                break;
            }
            case OP_NOT: {
                long long a = stack_pop();
                stack_push(~a);
                break;
            }
            case OP_SHL: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a << b);
                break;
            }
            case OP_SHR: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a >> b);
                break;
            }
            case OP_DUP: {
                stack_push(stack_peek());
                break;
            }
            case OP_SWAP: {
                long long a = stack_pop();
                long long b = stack_pop();
                stack_push(a);
                stack_push(b);
                break;
            }
            case OP_DROP: {
                stack_pop();
                break;
            }
            case OP_OVER: {
                if (stack_top < 1) panic("Stack underflow");
                stack_push(stack[stack_top - 1]);
                break;
            }
            case OP_ROT: {
                if (stack_top < 2) panic("Stack underflow");
                long long c = stack_pop();
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(b);
                stack_push(c);
                stack_push(a);
                break;
            }
            case OP_NIP: {
                if (stack_top < 1) panic("Stack underflow");
                long long t = stack_pop();
                stack_pop();
                stack_push(t);
                break;
            }
            case OP_TUCK: {
                if (stack_top < 1) panic("Stack underflow");
                long long a = stack_pop();
                long long b = stack_pop();
                stack_push(a);
                stack_push(b);
                stack_push(a);
                break;
            }
            case OP_STORE: {
                if (inst.val < 0 || inst.val >= MEM_SIZE) panic("Invalid register");
                registers[inst.val] = stack_pop();
                break;
            }
            case OP_LOAD: {
                if (inst.val < 0 || inst.val >= MEM_SIZE) panic("Invalid register");
                stack_push(registers[inst.val]);
                break;
            }
            case OP_EQ: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a == b ? 1 : 0);
                break;
            }
            case OP_NEQ: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a != b ? 1 : 0);
                break;
            }
            case OP_LT: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a < b ? 1 : 0);
                break;
            }
            case OP_GT: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a > b ? 1 : 0);
                break;
            }
            case OP_LTE: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a <= b ? 1 : 0);
                break;
            }
            case OP_GTE: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a >= b ? 1 : 0);
                break;
            }
            case OP_INC: {
                stack_push(stack_pop() + 1);
                break;
            }
            case OP_DEC: {
                stack_push(stack_pop() - 1);
                break;
            }
            case OP_ABS: {
                long long a = stack_pop();
                stack_push(a < 0 ? -a : a);
                break;
            }
            case OP_NEG: {
                stack_push(-stack_pop());
                break;
            }
            case OP_MIN: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a < b ? a : b);
                break;
            }
            case OP_MAX: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push(a > b ? a : b);
                break;
            }
            case OP_POW: {
                long long b = stack_pop();
                long long a = stack_pop();
                stack_push((long long)pow(a, b));
                break;
            }
            case OP_SQRT: {
                long long a = stack_pop();
                if (a < 0) panic("Negative sqrt");
                stack_push((long long)sqrt(a));
                break;
            }
            case OP_JMP: {
                int target = find_label(inst.label_name);
                if (target == -1) panic("Label not found");
                pc = target;
                continue;
            }
            case OP_JZ: {
                if (stack_pop() == 0) {
                    int target = find_label(inst.label_name);
                    if (target == -1) panic("Label not found");
                    pc = target;
                    continue;
                }
                break;
            }
            case OP_JNZ: {
                if (stack_pop() != 0) {
                    int target = find_label(inst.label_name);
                    if (target == -1) panic("Label not found");
                    pc = target;
                    continue;
                }
                break;
            }
            case OP_CALL: {
                int target = find_label(inst.label_name);
                if (target == -1) panic("Label not found");
                call_stack_push(pc + 1);
                pc = target;
                continue;
            }
            case OP_RET: {
                pc = call_stack_pop();
                continue;
            }
            case OP_LABEL:
            case OP_NOP:
                break;
            case OP_PRINT: {
                printf("%lld\n", stack_pop());
                break;
            }
            case OP_PRINTS: {
                printf("%c", (char)stack_pop());
                break;
            }
            case OP_INPUT: {
                long long val = 0;
                if (scanf("%lld", &val) != 1) panic("Invalid input");
                stack_push(val);
                break;
            }
            case OP_HALT:
                return;
            default:
                break;
        }
        pc++;
    }
}

int main(int argc, char *argv[]) {
    const char *filename = "example.app";
    if (argc > 1) {
        filename = argv[1];
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Could not open file %s\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return 0;
    }

    char *source = (char *)malloc(fsize + 1);
    if (!source) {
        fclose(f);
        panic("Memory allocation failed");
    }

    size_t len = fread(source, 1, fsize, f);
    source[len] = '\0';
    fclose(f);

    parse_esoteric_app(source);
    free(source);

    execute_program();

    if (stack_top >= 0) {
        printf("%lld\n", stack[stack_top]);
    }

    fflush(stdout);
    return 0;
}