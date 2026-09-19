#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 256
#define LIBS_DIR "libs/"

void trim(char *str) {
    char *p = str;
    int l = strlen(p);
    while (l > 0 && isspace((unsigned char)p[l - 1])) p[--l] = 0;
    while (*p && isspace((unsigned char)*p)) p++;
    memmove(str, p, l + 1);
}

void pause_and_exit(int code) {
    printf("\nPress Enter to exit...");
    fflush(stdout);
    getchar();
    getchar();
    exit(code);
}

void process_file(FILE *in, FILE *out, int *line_num) {
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), in)) {
        trim(line);

        if (line[0] == '\0') continue;

        char *comment = strstr(line, "--");
        if (comment) *comment = '\0';
        comment = strstr(line, "//");
        if (comment) *comment = '\0';
        
        if (line[0] == '#' && strncmp(line, "#include", 8) != 0) continue;

        trim(line);
        if (line[0] == '\0') continue;

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == ';') {
            line[len - 1] = '\0';
            trim(line);
        }

        char cmd[64] = {0};
        char raw_path[256] = {0};
        
        if (sscanf(line, "%63s %255s", cmd, raw_path) >= 2) {
            if (strcmp(cmd, "include") == 0 || strcmp(cmd, "#include") == 0) {
                
                char clean_name[256];
                int j = 0;
                for (int i = 0; raw_path[i] != '\0'; i++) {
                    if (raw_path[i] != '"' && raw_path[i] != '<' && raw_path[i] != '>' && raw_path[i] != ';') {
                        clean_name[j++] = raw_path[i];
                    }
                }
                clean_name[j] = '\0';

                char path[512];
                snprintf(path, sizeof(path), "%s%s", LIBS_DIR, clean_name);
                FILE *lib_file = fopen(path, "r");

                if (!lib_file) {
                    lib_file = fopen(clean_name, "r");
                }

                if (lib_file) {
                    printf(" -> Including library: %s\n", clean_name);
                    process_file(lib_file, out, line_num);
                    fclose(lib_file);
                } else {
                    printf(" Error: Could not find library '%s' in '%s' or current directory!\n", clean_name, LIBS_DIR);
                }
                continue;
            }
        }

        if (line[strlen(line) - 1] == ':') {
            fprintf(out, "%s;\n", line);
            continue;
        }

        char *endptr;
        long val = strtol(line, &endptr, 10);
        if (*endptr == '\0') {
            fprintf(out, "    push(%ldL);\n", val);
            continue;
        }

        if (strncmp(line, "jmp ", 4) == 0) {
            fprintf(out, "    goto %s;\n", line + 4);
            continue;
        }
        if (strncmp(line, "jz ", 3) == 0) {
            fprintf(out, "    if (pop() == 0) goto %s;\n", line + 3);
            continue;
        }

        if (strncmp(line, "call ", 5) == 0) {
            fprintf(out, "    call_stack[csp++] = &&ret_lbl_%d;\n", *line_num);
            fprintf(out, "    goto %s;\n", line + 5);
            fprintf(out, "    ret_lbl_%d:;\n", *line_num);
            (*line_num)++;
            continue;
        }

        if (strcmp(line, "ret") == 0) {
            fprintf(out, "    if (csp > 0) {\n");
            fprintf(out, "        goto *call_stack[--csp];\n");
            fprintf(out, "    } else { return 0; }\n");
            continue;
        }

        if (strcmp(line, "dup") == 0) fprintf(out, "    push(peek());\n");
        else if (strcmp(line, "drop") == 0) fprintf(out, "    pop();\n");
        else if (strcmp(line, "swap") == 0) fprintf(out, "    { long a=pop(), b=pop(); push(a); push(b); }\n");
        else if (strcmp(line, "over") == 0) fprintf(out, "    { long a=pop(), b=pop(); push(b); push(a); push(b); }\n");
        else if (strcmp(line, "add") == 0) fprintf(out, "    push(pop() + pop());\n");
        else if (strcmp(line, "sub") == 0) fprintf(out, "    { long b=pop(), a=pop(); push(a - b); }\n");
        else if (strcmp(line, "mul") == 0) fprintf(out, "    push(pop() * pop());\n");
        else if (strcmp(line, "div") == 0) fprintf(out, "    { long b=pop(), a=pop(); push(a / b); }\n");
        else if (strcmp(line, "lt") == 0) fprintf(out, "    { long b=pop(), a=pop(); push(a < b ? 1 : 0); }\n");
        else if (strcmp(line, "gt") == 0) fprintf(out, "    { long b=pop(), a=pop(); push(a > b ? 1 : 0); }\n");
        else if (strcmp(line, "lte") == 0) fprintf(out, "    { long b=pop(), a=pop(); push(a <= b ? 1 : 0); }\n");
        else if (strcmp(line, "print") == 0) fprintf(out, "    printf(\"%%ld\\n\", pop());\n");
        else if (strcmp(line, "halt") == 0) fprintf(out, "    return 0;\n");
        else fprintf(out, "    // Unknown instruction: %s\n", line);
    }
}

int main() {
    char input_filename[256];
    char c_filename[256] = "temp_output.c";
    char exe_filename[256];
    
    printf("Enter input file name (.app): ");
    if (scanf("%255s", input_filename) != 1) {
        printf("Input error.\n");
        pause_and_exit(1);
    }

    FILE *in = fopen(input_filename, "r");
    if (!in) {
        printf("Error: Could not open file '%s'\n", input_filename);
        pause_and_exit(1);
    }

    strcpy(exe_filename, input_filename);
    char *dot = strrchr(exe_filename, '.');
    if (dot) *dot = '\0';
    strcat(exe_filename, ".exe");

    FILE *out = fopen(c_filename, "w");
    if (!out) {
        printf("Error: Could not create temporary C file.\n");
        fclose(in);
        pause_and_exit(1);
    }

    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");
    fprintf(out, "#define STACK_SIZE 1024\n");
    fprintf(out, "long stack[STACK_SIZE];\n");
    fprintf(out, "int sp = 0;\n\n");
    fprintf(out, "void *call_stack[STACK_SIZE];\n");
    fprintf(out, "int csp = 0;\n\n");
    fprintf(out, "void push(long val) { stack[sp++] = val; }\n");
    fprintf(out, "long pop() { return stack[--sp]; }\n");
    fprintf(out, "long peek() { return stack[sp - 1]; }\n\n");
    fprintf(out, "int main() {\n");

    int line_num = 0;
    process_file(in, out, &line_num);

    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");

    fclose(in);
    fclose(out);

    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc %s -o %s", c_filename, exe_filename);

    printf("\n[1/2] Translation completed.\n");
    printf("[2/2] Compiling C -> %s...\n", exe_filename);

    int res = system(compile_cmd);
    
    remove(c_filename);

    if (res == 0) {
        printf("\nSuccess! Executable created: %s\n", exe_filename);
    } else {
        printf("\nError: Compilation failed.\n");
    }

    pause_and_exit(0);
    return 0;
}