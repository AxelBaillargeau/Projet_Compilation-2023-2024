#include "asm_writer.h"

#include <string.h>
#include <stdlib.h>
#include "symbole.h"
#include <regex.h>
#include <err.h>
#include <error.h>

#define MAX_IMMEDIATE 256

#define IS_NOT_FOUND(err, var, lineno, charno) \
    if (err == 2) err_prnt(SEM_ERR, lineno, charno, "Variable '%s' not found", var)

int debug = 0;

static unsigned int jump = 1;
static unsigned char has_main = 0;
static unsigned char has_return = 0;

typedef struct glbs_ {
    int size;
    char **names;
    int *sizes;
    Type *types;
} GLBS;

static void asm_from_tree(Node *tree, Context *cont, FILE *file, Type func_type);
static int trad_expr(Node *tree, Context *const cont, FILE *file, char *const reg_res, Type *type, char const push);
static void param_size_check(int read_size, int param_size, Type param_type, Node *param);
static void write_arg_passing(int read_size, int param_size, FILE *file, int offset, Type param_type);
static void crash(FILE *file, int sig) { fprintf(file, "\tmov rdi, %d\n\tmov rax, 60\n\tsyscall\n", sig); }
static void save_rbp(FILE *file) {
    fprintf(file, "\tpush rbp\n\tmov rbp, rsp");
    if (debug) fprintf(file, "; save rbp");
    fprintf(file, "\n");
}
static void restore_rbp(FILE *file) { fprintf(file, "\tleave\n"); }
static int closest_16(int n) { return n == 0 ? 0 : n + 15 - (n - 1) % 16; }
static void header_write(FILE *file) { fprintf(file, "global _start\nsection .text\n"); }
static void return_write(FILE *file) { fprintf(file, "\tmov rdi, rax\n\tmov rax, 60\n\tsyscall\n"); }
static void glb_header(FILE *file) { fprintf(file, "section .data\n"); }
static void call(FILE *file, Node *node) { fprintf(file, "\tcall %s\n", node->value); }

static int min(Type a, Type b) { return a < b ? a : b; }

static void prepare_stack(FILE *file, int size) {
    int n = closest_16(size);
    if (n > 0) {
        fprintf(file, "\tsub rsp, %d", n);
        if (debug) fprintf(file, "; prepare stack");
        fprintf(file, "\n");
    }
}

static void global_write(FILE *file, char *name, Type type, int size) {
    char write[3] = {"db"};
    if (size_of(type) == 4) write[1] = 'd';
    fprintf(file, "%s: %s %d DUP(0)\n\n", name, write, size);
}

static void decl_global(FILE *file, HashTable *table, GLBS *glb) {
    int size = 1;
    Type type;
    glb_header(file);
    glb->size = 0;
    glb->names = malloc(SYMBOLE_TABLE_MAX_SIZE * sizeof(char *));
    glb->sizes = malloc(SYMBOLE_TABLE_MAX_SIZE * sizeof(int));
    glb->types = malloc(SYMBOLE_TABLE_MAX_SIZE * sizeof(Type));
    if (glb->names == NULL || glb->sizes == NULL || glb->types == NULL) error(MISC_ERR, 0, "Memory allocation failed");
    for (int i = 0; i < SYMBOLE_TABLE_MAX_SIZE; i++) {
        Entry *entry = table->buckets[i];
        while (entry) {
            type = entry->value.type;
            if (type == TYPE_FUNCTION) {
                entry = entry->next;
                continue;
            }
            if (type == TYPE_CHAR_ARRAY || type == TYPE_INT_ARRAY) {
                size = entry->value.value.array.size;
                type -= 3;
                glb->names[glb->size] = entry->identifier;
                glb->sizes[glb->size] = size;
                glb->types[glb->size] = type;
                glb->size += 1;
                fprintf(file, "%s.adr: dd 2 DUP(0)\n", entry->identifier);
            };
            global_write(file, entry->identifier, type, size);
            size = 1;
            entry = entry->next;
        }
    }
}

static void assign_adrs(FILE *file, GLBS *glb) {
    for (int i = 0; i < glb->size; i++) {
        fprintf(file, "\tmov QWORD [%s.adr], %s\n", glb->names[i], glb->names[i]);
        // fprintf(file, "\tadd QWORD [%s.adr], %d\n", glb->names[i], (glb->sizes[i] - 2) * size_of(glb->types[i]));
        fprintf(file, "\tadd QWORD [%s.adr], %d\n", glb->names[i], (glb->sizes[i] - 1) * size_of(glb->types[i]));
    }
}

static void end_asm(FILE *file, Context *cont, GLBS *glb) {
    fprintf(file, "\n_start:\n");
    assign_adrs(file, glb);
    Node main_node = (Node){.value = "main"};
    call(file, &main_node);
    return_write(file);
}

static void oper_print(Node *tree, FILE *file) {
    switch (tree->value[0]) {
        case '-':
            fprintf(file, "\tsub eax, ebx\n");
            break;
        case '+':
            fprintf(file, "\tadd eax, ebx\n");
            break;
        case '*':
            fprintf(file, "\timul eax, ebx\n");
            break;
        case '/':
            fprintf(file, "\txor rdx, rdx\n");
            // cdq
            fprintf(file, "\tcqo\n");
            fprintf(file, "\tidiv rbx\n");
            break;
        case '%':
            fprintf(file, "\txor rdx, rdx\n");
            fprintf(file, "\tidiv rbx\n");
            fprintf(file, "\tmov eax, edx\n");
            break;
        case '|':
            fprintf(file, "\tor eax, ebx\n");
            break;
        case '&':
            fprintf(file, "\tand eax, ebx\n");
            break;
        default:
            err_prnt(SEM_ERR, tree->lineno, tree->charno, "Unknown operator %s", tree->value);
            break;
    }
}

static void write_order(int eq, int sup, FILE *file) {
    switch (sup) {
        case 1:
            if (eq)
                fprintf(file, "\tsetge al\n");
            else
                fprintf(file, "\tsetg al\n");
            break;
        case -1:
            if (eq)
                fprintf(file, "\tsetle al\n");
            else
                fprintf(file, "\tsetl al\n");
            break;
        default:
            if (eq)
                fprintf(file, "\tsete al\n");
            else
                fprintf(file, "\tsetne al\n");
            break;
    }
    fprintf(file, "\tmovzx eax, al\n");
}

static void get_var(Node *tree, Context *cont, FILE *file, int *offset, char *reg_res, Type *type) {
    int ret = 0;
    char reg[64] = "rbp";
    strcpy(reg, reg_res);
    // char modif[6] = "DWORD";
    if (tree->label == id) {
        ValueType left = search_var(*cont, tree->value, &ret);
        IS_NOT_FOUND(ret, tree->value, tree->lineno, tree->charno);
        *offset = left.size;
    } else if (tree->label == idArray) {  // Array
        ValueType left = search_var(*cont, tree->value, &ret);
        trad_expr(tree, cont, file, reg_res, type, 0);
        *offset = FIRSTCHILD(tree) != NULL ? size_of(*type) : size_of(left.type - 3);
    }
    trad_expr(tree, cont, file, reg_res, type, 0);
}

static void param_size_check(int read_size, int param_size, Type param_type, Node *param) {
    if (read_size != 0 && read_size != param_size) {
        if (param_type >= TYPE_INT_ARRAY && read_size < param_size)  // is array
            err_prnt(SEM_ERR, param->lineno, param->charno, "Passed value '%s' is not an array", param->value);
        err_prnt(0, param->lineno, param->charno, "Passed parameter '%s' has different size than expected",
                 param->value);
    }
}

static void write_arg_passing(int read_size, int param_size, FILE *file, int offset, Type param_type) {
    if (read_size == 1 || param_size == 1)
        fprintf(file, "\tmov BYTE [rsp-%d], dl\n", 16 + offset);
    else if (read_size == 4 || param_size == 4)
        fprintf(file, "\tmov DWORD [rsp-%d], edx\n", 16 + offset);
    else
        fprintf(file, "\tmov QWORD [rsp-%d], rdx\n", 16 + offset);
}

static void function_assemble(Node *tree, Context *cont, FILE *file, Type *type) {
    int param_size = 0, i = 0, offset = 0, read_size = 0;
    // char *reg[6] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"}; // TODO: use registers
    Node *id = FIRSTCHILD(tree), *args = SECONDCHILD(tree), *param = NULL;
    if (args != NULL) param = FIRSTCHILD(args);
    ValueType val = table_get(cont->global, id->value);
    Function f = val.value.function_val;
    if (val.type != (Type)TYPE_FUNCTION)
        err_prnt(SEM_ERR, id->lineno, id->charno, "Function '%s' not declared", id->value);
    // if (f.params == NULL && param != NULL)
    //     err_prnt(SEM_ERR, id->lineno, id->charno, "Function '%s' takes no parameters", id->value);

    *type = min(*type, f.return_type);
    while (param != NULL) {
        Type passed_type, param_type = f.params[i].type;
        param_size = f.params[i++].size;
        if (i > f.nb_params)
            err_prnt(SEM_ERR, id->lineno, id->charno, "Too many parameters for function '%s'", id->value);
        offset += param_size;
        get_var(param, cont, file, &read_size, "rdx", &passed_type);
        param_size_check(read_size, param_size, param_type, param);
        write_arg_passing(read_size, param_size, file, offset, param_type);
        read_size = 0;
        param = param->nextSibling;
    }
    call(file, id);
}

static void push_literal_char(FILE *file, char c) {
    switch (c) {
        case '\0':
            fprintf(file, "0\n");
            return;
        case '\a':
            fprintf(file, "7\n");
            return;
        case '\b':
            fprintf(file, "8\n");
            return;
        case '\t':
            fprintf(file, "9\n");
            return;
        case '\n':
            fprintf(file, "10\n");
            return;
        case '\v':
            fprintf(file, "11\n");
            return;
        case '\f':
            fprintf(file, "12\n");
            return;
        case '\r':
            fprintf(file, "13\n");
            return;

        default:
            break;
    }
    fprintf(file, "'%c'\n", c);
}

static void trad_or_expr(Node *tree, Context *const cont, FILE *file, char *const reg_res, Type *type) {
    Type tmp = 0;
    int skip = jump++, save = strcmp("rax", reg_res) != 0;
    // if (strcmp(FIRSTCHILD(tree)->value, "0") == 0)
    //     err_prnt(0, tree->lineno, tree->charno, "Will always evaluate to true");

    if (save) fprintf(file, "\tpush rax\n");

    trad_expr(FIRSTCHILD(tree), cont, file, "rcx", &tmp, 0);
    fprintf(file, "\txor rax, rax\n\tcmp rcx, 0\n\tsetne al\n\tpush rax\n\tjne .OrSkip%d\n", skip);

    trad_expr(SECONDCHILD(tree), cont, file, "rdx", type, 0);
    fprintf(file, "\txor rbx, rbx\n\tcmp rdx, 0\n\tsetne bl\n\tpop rax\n");

    oper_print(tree, file);

    fprintf(file, "\tpush rax\n.OrSkip%d:\n\tpop rax\n", skip);
    if (save) fprintf(file, "\tmov %s, rax\n\tpop rax\n", reg_res);
    *type = min(tmp, *type);  // because TYPE_INT < TYPE_CHAR
}

static void trad_and_expr(Node *tree, Context *const cont, FILE *file, char *const reg_res, Type *type) {
    Type tmp = 0;
    int skip = jump++, save = strcmp("rax", reg_res) != 0;
    if (strcmp(FIRSTCHILD(tree)->value, "0") == 0 || FIRSTCHILD(tree)->value[0] == 0)
        err_prnt(0, tree->lineno, tree->charno, "Will always evaluate to false");

    if (save) fprintf(file, "\tpush rax\n");
    trad_expr(FIRSTCHILD(tree), cont, file, "rcx", &tmp, 0);
    fprintf(file, "\txor rax, rax\n\tcmp rcx, 0\n\tsetne al\n\tpush rax\n\tje .AndSkip%d\n", skip);
    trad_expr(SECONDCHILD(tree), cont, file, "rdx", type, 0);
    fprintf(file, "\txor rbx, rbx\n\tcmp rdx, 0\n\tsetne bl\n.AndSkip%d:\n\tpop rax\n", skip);
    oper_print(tree, file);
    if (save) fprintf(file, "\tmov %s, rax\n\tpop rax\n", reg_res);
    *type = min(tmp, *type);  // because TYPE_INT < TYPE_CHAR
}

static void cltwopwr(int n, int *i, int *res, int *prec) {
    *res = 1, *prec = 0, *i = 0;
    while (*res < n) {
        *prec = *res;
        *res *= 2;
        (*i)++;
    }
}

static int dist(int a, int b) { return a > b ? a - b : b - a; }

static int closest(int n, int *dst, int *i) {
    int res, prec;
    cltwopwr(n, i, &res, &prec);
    int d1 = dist(n, prec), d2 = dist(n, res);
    if (d1 < d2) {
        *dst = d1;
        return 0;
    }
    *dst = d2;
    return 1;
}

static int magic_bitshift(FILE *file, char *reg_res, int div, int value) {
    if (value == 0) {
        fprintf(file, "\txor %s, %s", reg_res, reg_res);  // forcement une multiplication par 0
        return 1;
    }
    int dst, shift;
    closest(value, &dst, &shift);
    if (dst != 0) return 0;
    if (div)
        fprintf(file, "\tsar %s, %d\n", reg_res, shift);
    else
        fprintf(file, "\tshl %s, %d\n", reg_res, shift);
    return 1;
}

static int trad_expr(Node *tree, Context *const cont, FILE *file, char *const reg_res, Type *type, char const push) {
    if (tree == NULL) return 0;
    int eq = 0, sup = 0, offset = 0;
    *type = ~(-0);
    ValueType left;
    switch (tree->label) {
        case Number:
            *type = TYPE_INT;
            fprintf(file, "\tmov %s, %s\n", reg_res, tree->value);
            break;
        case Character:
            fprintf(file, "\tmov %s, ", reg_res);
            push_literal_char(file, tree->value[0]);
            *type = TYPE_CHAR;
            break;
        case EqOp:
        case Order:
            sup += -1 * (tree->value[0] == '<') + (tree->value[0] == '>');
            if (tree->value[1] == '=') eq = 1 - (tree->value[0] == '!');
            trad_expr(SECONDCHILD(tree), cont, file, "rbx", type, 1);
            trad_expr(FIRSTCHILD(tree), cont, file, "rax", type, 0);
            fprintf(file, "\tpop rbx\n");
            fprintf(file, "\tcmp rax, rbx\n");
            write_order(eq, sup, file);
            if (strcmp(reg_res, "rax") != 0) fprintf(file, "\tmov %s, rax\n", reg_res);
            *type = TYPE_INT;
            break;
        case DivStar:
            if (tree->value[0] == '/' && SECONDCHILD(tree)->label == Number && atoi(SECONDCHILD(tree)->value) == 0)
                err_prnt(SEM_ERR, tree->lineno, tree->charno, "Division by zero");

            trad_expr(FIRSTCHILD(tree), cont, file, reg_res, type, 1);
            {
                Node *right = SECONDCHILD(tree);
                if (right->label == Number || right->label == Character) {
                    int value = right->label == Number ? atoi(right->value) : right->value[0];
                    if (magic_bitshift(file, reg_res, tree->value[0] == '/', value)) break;
                }
            }
            trad_expr(SECONDCHILD(tree), cont, file, "rbx", type, 0);
            fprintf(file, "\tpop rax\n");
            oper_print(tree, file);
            if (strcmp(reg_res, "rax") != 0) fprintf(file, "\tmov %s, rax\n", reg_res);
            break;
        case OrOp:
            trad_or_expr(tree, cont, file, reg_res, type);
            break;
        case AndOp:
            trad_and_expr(tree, cont, file, reg_res, type);
            break;
        case AddSub: {
            Type tmp = 0;
            // no need to check for chars since they always fit
            Node *right = SECONDCHILD(tree);
            trad_expr(FIRSTCHILD(tree), cont, file, reg_res, type, 0);
            if (right->label == Number || right->label == Character) {
                char *opers[4] = {"add", "sub"};
                tmp = TYPE_CHAR;
                if (right->label == Number) {
                    fprintf(file, "\t%s %s, %s\n", opers[tree->value[0] == '-'], reg_res, SECONDCHILD(tree)->value);
                    tmp = TYPE_INT;
                } else
                    fprintf(file, "\t%s %s, %d\n", opers[tree->value[0] == '-'], reg_res, SECONDCHILD(tree)->value[0]);
                *type = min(tmp, *type);
                break;
            }
            fprintf(file, "\tpush %s\n", reg_res);
            trad_expr(SECONDCHILD(tree), cont, file, "rbx", &tmp, 0);
            fprintf(file, "\tpop rax\n");
            // fprintf(file, "\tpop rbx\n\tpop rax\n");
            *type = min(tmp, *type);
            oper_print(tree, file);
            if (strcmp(reg_res, "rax") != 0) fprintf(file, "\tmov %s, rax\n", reg_res);
        } break;
        case UnaryOp: {
            trad_expr(FIRSTCHILD(tree), cont, file, reg_res, type, 0);
            fprintf(file, "\tcmp rax, 0\n");
            fprintf(file, "\tsete al\n");
            fprintf(file, "\tmovzx eax, al\n");
        } break;
        case idArray: {  // quand on accède à un index d'un tableau
            Node *ind = FIRSTCHILD(tree);
            int index, ret = 0, size, is_access = ind != NULL;
            char /* place[10] = "rbp", */ *size_str = "DWORD", reg[10] = "edx";
            strncpy(reg + 1, reg_res + 1, 3);
            left = search_var(*cont, tree->value, &ret);
            size = size_of(left.type - 3);
            offset = 0;
            if (debug) fprintf(file, "\t;access idArray %s[%s]\n", tree->value, ind->value);
            fprintf(file, "\tmov rcx, ");

            IS_NOT_FOUND(ret, tree->value, tree->lineno, tree->charno);
            else if (ret == 1) fprintf(file, "[%s.adr]\n", tree->value);
            else fprintf(file, "[rbp-%ld]\n", left.address + left.size);

            if (size == 1 && is_access) {
                size_str = "BYTE";
                strcpy(reg, reg_res + 1);
                reg[1] = 'l';
            }
            index = ind->value[0];
            switch (ind->label) {
                case Number:
                    index = atoi(ind->value);
                case Character:
                    offset = size * index;
                    if (index < 0 || (index >= left.value.array.size && left.value.array.size != -1))
                        err_prnt(SEM_ERR, ind->lineno, ind->charno, "Index %d out of range", index);
                    if (offset != 0) fprintf(file, "\tsub rcx, %d\n", offset);
                    break;

                default:  // index is an expression
                    index = 0;
                    trad_expr(ind, cont, file, "rdx", type, 0);
                    fprintf(file, "\timul rdx, %d\n\tsub rcx, rdx\n", size);
                    break;
            }
            fprintf(file, "\tmov %s, %s [rcx]\n", reg, size_str);

            *type = min(left.type - 3, *type);
        } break;
        case id: {
            int ret = 0;
            char place[64] = "rbp", *size_str = "DWORD", reg[64] = "edx";
            strncpy(reg + 1, reg_res + 1, 3);
            left = search_var(*cont, tree->value, &ret);
            offset = left.address + left.size;
            IS_NOT_FOUND(ret, tree->value, tree->lineno, tree->charno);
            else if (ret == 1) {  // global variable
                if (left.type == TYPE_FUNCTION)
                    err_prnt(SEM_ERR, tree->lineno, tree->charno, "Cannot use function '%s' as variable", tree->value);
                offset = left.size;
                strcpy(place, tree->value);
            }
            if (left.size == 1) {
                size_str = "BYTE";
                // strcpy(mov_oper, "movzx");
                strcpy(reg, reg_res + 1);
                reg[1] = 'l';
            } else if (left.size == 8) {
                size_str = "QWORD";
                reg[0] = 'r';
            }

            fprintf(file, "\tmov %s, %s [%s-%d]\n", reg, size_str, place, offset);

            *type = min(left.type, *type);
        } break;
        case FunctionCall: {
            ValueType val = table_get(cont->global, FIRSTCHILD(tree)->value);
            if (val.type != (Type)TYPE_FUNCTION)
                err_prnt(SEM_ERR, tree->lineno, tree->charno, "Function '%s' not declared", FIRSTCHILD(tree)->value);
            if (val.value.function_val.return_type == TYPE_VOID)
                err_prnt(SEM_ERR, tree->lineno, tree->charno, "Function '%s' does not return a value",
                         FIRSTCHILD(tree)->value);
            function_assemble(tree, cont, file, type);
            if (strcmp(reg_res, "rax") != 0) fprintf(file, "\tmov %s, rax\n", reg_res);
        } break;
        default:
            err_prnt(SEM_ERR, tree->lineno, tree->charno, "Unknown node type: %d", tree->label);
            break;
    }
    if (push) fprintf(file, "\tpush %s\n", reg_res);
    return 0;
}

static void assign_array(Node *tree, Context *cont, FILE *file, Type *type, ValueType left, int glb) {
    int offset = 0, size = size_of(left.type - 3), index;
    Node *left_node = FIRSTCHILD(tree), *right_node = SECONDCHILD(tree);
    Node *ind = FIRSTCHILD(FIRSTCHILD(tree));

    if (debug) fprintf(file, "\t;assign array %s\n", left_node->value);
    // mov rcx, rbp
    // sub rcx, address
    // sub rcx, size(type.left-3)

    trad_expr(right_node, cont, file, "rax", type, 1);

    if (glb == 1) {
        fprintf(file, "\tmov rcx, [%s.adr]\n", left_node->value);
        offset -= left.address;  // should already be 0 be just in case
    } else
        fprintf(file, "\tmov rcx, [rbp-%ld]\n", left.address + left.size);

    if (ind->label == (label_t)Number || ind->label == Character) {
        index = ind->label == Number ? atoi(ind->value) : ind->value[0];
        if (index < 0 || (index >= left.value.array.size && left.value.array.size != -1))
            err_prnt(SEM_ERR, ind->lineno, ind->charno, "Index %d out of range", index);
    } else {
        index = 0;
        trad_expr(ind, cont, file, "rbx", type, 0);
        fprintf(file, "\timul rbx, %d\n", size);
        fprintf(file, "\tsub rcx, rbx\n");
    }
    offset += size * index;  // tab[0] == *tab == [rbp-4], tab[1] = *(tab+1)
    fprintf(file, "\tpop rax\n");
    if (left.type == TYPE_CHAR || left.type == TYPE_CHAR_ARRAY)
        fprintf(file, "\tmov BYTE [rcx-%d], al\n", offset);
    else
        fprintf(file, "\tmov DWORD [rcx-%d], eax\n", offset);

    if (size_of(left.type - 3) > size_of(*type))
        err_prnt(0, tree->lineno, tree->charno, "char assignment with int value");
    if (index < 0 || index >= left.value.array.size)
        err_prnt(SEM_ERR, ind->lineno, ind->charno, "Index %d out of range", index);
}

static void assign_value(Node *tree, Context *cont, FILE *file, Type *type, ValueType left, int glb) {
    if (left.type >= TYPE_INT_ARRAY) return assign_array(tree, cont, file, type, left, glb);

    trad_expr(SECONDCHILD(tree), cont, file, "rax", type, 0);

    char *size_modif = "DWORD";
    char *reg_res = "eax";
    if (left.size == 1) {
        reg_res = "al";
        size_modif = "BYTE";
    }
    if (glb == 1) {
        fprintf(file, "\tmov %s [%s.adr], %s\n", size_modif, tree->value, reg_res);
    } else {
        fprintf(file, "\tmov %s [rbp-%ld], %s\n", size_modif, left.address + left.size, reg_res);
    }
}

static int params_size(Function *func) {
    int size = 0;
    for (int i = 0; i < func->nb_params; i++) size += func->params[i].size;
    return size;
}

static void init_vars(Node *tree, Context *cont, FILE *file) {
    Node *type = FIRSTCHILD(tree);
    Node *id = NULL;
    // Node *size_arr = NULL;
    ValueType val;
    int glb, /* is_array = 0, */ tmp;
    // Type t = type->value[0] == 'i' ? TYPE_INT : TYPE_CHAR;
    while (type) {
        ;
        id = FIRSTCHILD(type);
        while (id) {
            // is_array = (size_arr = FIRSTCHILD(id)) != NULL;

            val = search_var(*cont, id->value, &glb);
            IS_NOT_FOUND(glb, id->value, id->lineno, id->charno);
            else if (glb == 1)
                err_prnt(SEM_ERR, id->lineno, id->charno, "Global variable '%s' already declared", id->value);
            tmp = val.address + val.size;
            if (val.type >= TYPE_INT_ARRAY) {
                fprintf(file, "\tmov rax, rbp\n");
                fprintf(file, "\tsub rax, %d\n", tmp + size_of(val.type - 3));
                fprintf(file, "\tmov [rbp-%d], QWORD rax", tmp);
            } else {
                fprintf(file, "\tmov [rbp-%d], ", tmp);
                if (val.type == TYPE_INT)
                    fprintf(file, "DWORD 0");
                else
                    fprintf(file, "BYTE 0");
            }
            if (debug) fprintf(file, " ;init var %s", id->value);
            fprintf(file, "\n");
            id = id->nextSibling;
        }
        type = type->nextSibling;
    }
}

static void asm_from_tree(Node *tree, Context *cont, FILE *file, Type func_type) {
    if (tree == NULL) return;
    Node *son = FIRSTCHILD(tree);
    Type type = 0;
    while (son != NULL) {
        switch (son->label) {
            case DecVars:
                if (cont->global != cont->local) init_vars(son, cont, file);
                break;
            case DecFoncts:
            case Body:
                asm_from_tree(son, cont, file, 0);
                break;
            case Fonction: {
                Node *ident = NULL, *entete = NULL, *corps = NULL;
                entete = FIRSTCHILD(son);
                ident = SECONDCHILD(entete);
                corps = SECONDCHILD(son);
                fprintf(file, "%s:\n", ident->value);
                Entry *a = table_get_adr(cont->global, ident->value);
                Function f = a->value.value.function_val;
                Context tmp = {cont->global, f.local_table};
                save_rbp(file);
                prepare_stack(file, a->value.size + params_size(&f));
                asm_from_tree(corps, &tmp, file, /* a->value.size, */ f.return_type);
                if (!has_return) {
                    if (f.return_type != TYPE_VOID)
                        err_prnt(SEM_ERR, ident->lineno, ident->charno, "Function '%s' does not return a value",
                                 ident->value);
                    restore_rbp(file);
                    fprintf(file, "\tret\n");
                }
                if (strcmp(ident->value, "main") == 0) {
                    has_main = 1;
                    if (f.return_type != TYPE_INT)
                        err_prnt(SEM_ERR, ident->lineno, ident->charno, "return type of 'main' is not 'int'");
                }
                has_return = 0;
            } break;
            case OrOp:
            case AndOp:
            case EqOp:
            case Order:
                trad_expr(son, cont, file, "rax", &type, 1);
                break;
            case Assign: {
                // char place[64] = {"rbp"};
                int ret = 0;
                ValueType left = search_var(*cont, FIRSTCHILD(son)->value, &ret);
                IS_NOT_FOUND(ret, FIRSTCHILD(son)->value, FIRSTCHILD(son)->lineno, FIRSTCHILD(son)->charno);
                if (left.type == TYPE_FUNCTION)
                    err_prnt(SEM_ERR, FIRSTCHILD(son)->lineno, FIRSTCHILD(son)->charno,
                             "Function '%s' cannot be assigned", FIRSTCHILD(son)->value);
                // else if (left.type >= TYPE_INT_ARRAY)
                //     assign_array(son, cont, file, &type, left, ret);
                // else {
                //     trad_expr(SECONDCHILD(son), cont, file, "rax", &type, 0);
                //     fprintf(file, "\tmov DWORD [rbp-%ld], eax\n", left.address + left.size);
                //     break;
                // }
                assign_value(son, cont, file, &type, left, ret);
            } break;
            case FunctionCall: {
                Type type;
                function_assemble(son, cont, file, &type);
            } break;
            case ReturnNode:
                if (FIRSTCHILD(son) != NULL) {
                    if (func_type == TYPE_VOID)
                        err_prnt(SEM_ERR, son->lineno, son->charno, "return value in void function");
                    else
                        trad_expr(FIRSTCHILD(son), cont, file, "rax", &type, 0);
                }
                restore_rbp(file);
                fprintf(file, "\tret\n");
                if (size_of(func_type) < size_of(type))
                    err_prnt(SEM_ERR, son->lineno, son->charno, "return type size mismatch");
                has_return = 1;
                break;
            case IfNode: {
                Node *cond = FIRSTCHILD(son), *stmt = SECONDCHILD(son);
                Node *else_stmt = stmt ? THIRDCHILD(son) : NULL;
                int start = jump++, end;
                if (else_stmt) end = jump++;
                trad_expr(cond, cont, file, "rax", &type, 0);
                if (type >= TYPE_INT_ARRAY)
                    err_prnt(SEM_ERR, cond->lineno, cond->charno, "Incorrect value used in condition");
                fprintf(file, "\tcmp rax, 0\n");
                fprintf(file, "\tje .IfSkip%d\n", start);
                asm_from_tree(stmt, cont, file, func_type);
                if (else_stmt) fprintf(file, "\tjmp .ElseSkip%d\n", end);
                fprintf(file, ".IfSkip%d:\n", start);
                asm_from_tree(else_stmt, cont, file, func_type);
                if (else_stmt) fprintf(file, ".ElseSkip%d:\n", end);
            } break;
            case WhileNode: {
                Node *cond = FIRSTCHILD(son), *stmt = SECONDCHILD(son);
                int start = jump++, end = jump++;
                fprintf(file, ".While%d:\n", start);
                trad_expr(cond, cont, file, "rax", &type, 0);
                if (type >= TYPE_INT_ARRAY)
                    err_prnt(SEM_ERR, cond->lineno, cond->charno, "Incorrect value used in condition");
                fprintf(file, "\tcmp rax, 0\n");
                fprintf(file, "\tje .WhileEnd%d\n", end);
                asm_from_tree(stmt, cont, file, func_type);
                fprintf(file, "\tjmp .While%d\n", start);
                fprintf(file, ".WhileEnd%d:\n", end);
            } break;
            // case UnaryOp:
            // case AddSub:
            // case DivStar:
            default:
                asm_from_tree(son, cont, file, func_type);
                break;
        }
        type = 0;
        son = son->nextSibling;
    }
}

static void write_stds(FILE *file) {
    fprintf(file, "getchar:\n");
    fprintf(file,
            "\tmov rax, 0\n\tmov rdi, 0\n\tpush byte 0\n\tmov rsi, rsp\n\tmov rdx, 1\n\tsyscall\n\tpop "
            "rax\n\tret\n");

    fprintf(file, "putchar:\n");
    fprintf(file,
            "\tmov rsi, rsp\n\tsub rsi, 9\n\tmov rax, 1\n\tmov rdi, 1\n\tmov rdx, "
            "1\n\tsyscall\n\tret\n");

    fprintf(file, "getint:\n");
    // fprintf(file,
    //         "\tpush rbp\n\tmov rbp, rsp\n\tsub rsp, 16\n\tmov DWORD [rbp-8], 1\n\tmov BYTE [rbp-1], 0\n\tjmp "
    //         ".getint1\n.getint2:\n\tmovzx eax, BYTE [rbp-1]\n\tmov BYTE [rbp-2], al\n\tcall getchar\n\tmov "
    //         "BYTE "
    //         "[rbp-1], "
    //         "al\n.getint1:\n\tcmp BYTE [rbp-1], '0'\n\tjl .getint2\n\tcmp BYTE [rbp-1], '9'\n\tjg "
    //         ".getint2\n\tcmp BYTE "
    //         "[rbp-2], '-'\n\tjne .getint3\n\tmov DWORD [rbp-8], -1\n.getint3:\n\tmovsx eax, BYTE "
    //         "[rbp-1]\n\tsub eax, "
    //         "48\n\tmov DWORD [rbp-12], eax\n\tmov DWORD [rbp-16], 0\n\tjmp .getint4\n.getint7:\n\tcall "
    //         "getchar\n\tmov "
    //         "BYTE [rbp-1], al\n\tcmp BYTE [rbp-1], '0'\n\tjl .getint5\n\tcmp BYTE [rbp-1], '9'\n\tjle "
    //         ".getint6\n.getint5:\n\tmov DWORD [rbp-16], 1\n\tjmp .getint4\n.getint6:\n\tmov edx, DWORD "
    //         "[rbp-12]\n\tmov "
    //         "eax, edx\n\tsal eax, 2\n\tadd eax, edx\n\tadd eax, eax\n\tmov edx, eax\n\tmovsx eax, BYTE "
    //         "[rbp-1]\n\tadd "
    //         "eax, edx\n\tsub eax, 48\n\tmov DWORD [rbp-12], eax\n.getint4:\n\tcmp DWORD [rbp-16], 0\n\tje "
    //         ".getint7\n\tmov eax, DWORD [rbp-12]\n\timul eax, DWORD [rbp-8]\n\tleave\n\tret\n");
    fprintf(
        file,
        "\tpush rbp\n\tmov rbp, rsp\n\tsub rsp, 16\n\tmov rax, 1\n\tmov DWORD [rbp-6], eax\n\tcall getchar\n\tmov BYTE "
        "[rbp-1], al\n\tmov rbx, '-'\n\tmov al, BYTE [rbp-1]\n\tcmp rax, rbx\n\tsete al\n\tmovzx eax, al\n\tcmp rax, "
        "0\n\tje .gtIfSkip1\n\tmov rax, -1\n\tmov DWORD [rbp-6], eax\n\tjmp .gtElseSkip2\n.gtIfSkip1:\n\tmov rbx, "
        "'0'\n\tmov al, BYTE [rbp-1]\n\tcmp rax, rbx\n\tsetl al\n\tmovzx eax, al\n\tmov rcx, rax\n\txor rax, "
        "rax\n\tcmp rcx, 0\n\tsetne al\n\tpush rax\n\tjne .gtOrSkip4\n\tmov rbx, '9'\n\tmov al, BYTE [rbp-1]\n\tcmp "
        "rax, rbx\n\tsetg al\n\tmovzx eax, al\n\tmov rdx, rax\n\txor rbx, rbx\n\tcmp rdx, 0\n\tsetne bl\n\tpop "
        "rax\n\tor eax, ebx\n\tpush rax\n.gtOrSkip4:\n\tpop rax\n\tcmp rax, 0\n\tje .gtElseSkip2\n\tmov rdi, 5\n    "
        "mov rax, 60\n    syscall\n.gtElseSkip2:\n\tmov al, BYTE [rbp-1]\n\tsub rax, 48\n\tmov DWORD [rbp-10], "
        "eax\n.gtWhile5:\n\tmov eax, DWORD [rbp-14]\n\tcmp rax, 0\n\tsete al\n\tmovzx eax, al\n\tcmp rax, 0\n\tje "
        ".gtWhileEnd6\n\tcall getchar\n\tmov BYTE [rbp-1], al\n\tmov rbx, '0'\n\tmov al, BYTE [rbp-1]\n\tcmp rax, "
        "rbx\n\tsetl al\n\tmovzx eax, al\n\tmov rcx, rax\n\txor rax, rax\n\tcmp rcx, 0\n\tsetne al\n\tpush rax\n\tjne "
        ".gtOrSkip9\n\tmov rbx, '9'\n\tmov al, BYTE [rbp-1]\n\tcmp rax, rbx\n\tsetg al\n\tmovzx eax, al\n\tmov rdx, "
        "rax\n\txor rbx, rbx\n\tcmp rdx, 0\n\tsetne bl\n\tpop rax\n\tor eax, ebx\n\tpush rax\n.gtOrSkip9:\n\tpop "
        "rax\n\tcmp rax, 0\n\tje .gtIfSkip7\n\tmov rax, 1\n\tmov DWORD [rbp-14], eax\n\tjmp "
        ".gtElseSkip8\n.gtIfSkip7:\n\tmov eax, DWORD [rbp-10]\n\tmov rbx, 10\n\timul eax, ebx\n\tmov bl, BYTE "
        "[rbp-1]\n\tadd eax, ebx\n\tsub rax, 48\n\tmov DWORD [rbp-10], eax\n.gtElseSkip8:\n\tjmp "
        ".gtWhile5\n.gtWhileEnd6:\n\tmov eax, DWORD [rbp-10]\n\tmov ebx, DWORD [rbp-6]\n\timul eax, "
        "ebx\n\tleave\n\tret\n");
    fprintf(file, "putint:\n");
    fprintf(
        file,
        "\tpush rbp\n\tmov rbp, rsp; save rbp\n\tsub rsp, 5; prepare stack\n\tcmp DWORD [rbp-4], "
        "0\n\tsetl al\n\tcmp al, 0\n\tje .putintL2\n\tmov BYTE [rsp-17], '-'\n\tcall putchar\n\tmov eax, DWORD "
        "[rbp-4]\n\tneg rax\n\tmov DWORD [rbp-4], eax\n\tjmp .putintL1\n.putintL2:\n\tmov eax, DWORD [rbp-4]\n\tcmp "
        "rax, 0\n\tsete al\n\tcmp al, 0\n\tje .putintL3\n\tmov BYTE [rsp-17], '0'\n\tcall putchar\n\tjmp "
        ".putintL1\n.putintL3:\n\tmov edx, DWORD [rbp-4]\n\tmov DWORD [rsp-20], edx\n\tcall "
        ".putint_sub\n.putintL1:\n\tleave\n\tret\n.putint_sub:\n\tpush rbp\n\tmov rbp, rsp; save rbp\n\tsub rsp, 4; "
        "prepare stack\n\tmov eax, DWORD [rbp-4]\n\tcmp rax, 0\n\tsete al\n\tcmp al, 0\n\tjne .putint_subL1\n\tmov "
        "eax, DWORD [rbp-4]\n\txor rdx, rdx\n\tmov rbx, 10\n\tidiv rbx\n\tpush rdx\n\tmov DWORD [rsp-20], eax\n\tcall "
        ".putint_sub\n\tpop rdx\n\tadd edx, '0'\n\tmov BYTE [rsp-17], dl\n\tcall "
        "putchar\n.putint_subL1:\n\tleave\n\tret\n");
}

static void add_std_func(HashTable *symbols) {
    ValueType a = create_value(TYPE_FUNCTION, 0), b = create_value(TYPE_FUNCTION, 0);
    a.value.function_val.return_type = TYPE_CHAR, b.value.function_val.return_type = TYPE_INT;
    table_put(symbols, "getchar", a);
    table_put(symbols, "getint", b);
    a = create_value(TYPE_FUNCTION, 0), b = create_value(TYPE_FUNCTION, 0);

    add_param(&a.value.function_val, TYPE_CHAR);

    add_param(&b.value.function_val, TYPE_INT);
    table_put(symbols, "putchar", a);
    table_put(symbols, "putint", b);
}

void asm_gen(Node *tree, HashTable *symbols, FILE *file) {
    GLBS glb;
    decl_global(file, symbols, &glb);
    header_write(file);
    write_stds(file);
    fprintf(file, "\n");
    add_std_func(symbols);
    Context cont = {symbols, symbols};
    asm_from_tree(tree, &cont, file, 0);
    end_asm(file, &cont, &glb);
    free(glb.names);
    free(glb.sizes);
    free(glb.types);
    if (!has_main) error(SEM_ERR, 0, "No main function found");
    // second_pass(file);
}