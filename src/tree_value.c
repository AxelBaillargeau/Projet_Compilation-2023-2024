#include "tree_value.h"

// #include <err.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static ValueType* create_params(Node* params, uint64_t* nb_params, HashTable* ht) {
    Node* param = FIRSTCHILD(params);
    Node* id = FIRSTCHILD(param);
    int max_params = 100;
    int adr = 0;
    ValueType t, *params_array = calloc(max_params, sizeof(ValueType));
    if (!params_array) error(MISC_ERR, 0, "calloc");
    while (param != NULL) {
        if (*nb_params >= max_params) {
            params_array = realloc(params_array, (max_params *= 2) * sizeof(ValueType));
            if (!params_array) error(MISC_ERR, 0, "realloc");
        }
        id = FIRSTCHILD(param);
        t = create_value(type_from_string(param->value), 0);
        if (id->label == idArray) t.type += 3;
        t.address = adr;
        t.size = size_of(t.type);
        adr += t.size;
        t.value.array.size = 0;
        params_array[*nb_params] = t;
        table_put(ht, id->value, t);
        (*nb_params)++;
        param = param->nextSibling;
    }
    return realloc(params_array, *nb_params * sizeof(ValueType));
}

/*
PROG
└── DecVars
    ├── type int
    │   ├── id a
    │   └── id b
    ├── type char
    │   ├── id c
    │   └── id d
    └── type int
        └── id e
            └── size 3
 */

static int put_vars(Node* type_node, HashTable* ht) {
    Type type = type_from_string(type_node->value);
    Node* id_node = FIRSTCHILD(type_node);
    int res = 0;
    while (id_node != NULL) {
        uint64_t arr_size = 0;
        if (FIRSTCHILD(id_node) != NULL && FIRSTCHILD(id_node)->label == (label_t)size) {
            arr_size = atoi(FIRSTCHILD(id_node)->value);
            res = 1;
            if (arr_size <= 0)
                error(SEM_ERR, 0, "Array size must be positive: line %d-%d", id_node->lineno, id_node->charno);
        }
        ValueType v = create_value(type, arr_size);
        table_put(ht, id_node->value, v);

        id_node = id_node->nextSibling;
    }
    return res;
}

static void put_vars_void(Node* type_node, HashTable* ht) { put_vars(type_node, ht); }

static int iterate_vars(Node* type_node, HashTable* ht) {
    Node* curr = type_node;
    int res = 0;
    while (curr != NULL) {
        res = put_vars(curr, ht) || res;
        curr = curr->nextSibling;
    }
    return res;
}

Function create_function_decl(Node* head, Node* body) {
    /* In functions, variable declaration are at the start of the base block (not decl in ifs for exemple) */
    Node* returnType = FIRSTCHILD(head);
    Node* decl = SECONDCHILD(head);
    Function f = create_function(returnType->value, decl->value);

    f.local_table = table_init();
    Node* params = decl->nextSibling;
    if (FIRSTCHILD(params) != NULL && FIRSTCHILD(params)->label == voidType)
        f.params = NULL;
    else {
        f.params = create_params(params, &f.nb_params, f.local_table);
        if (f.params == NULL) error(MISC_ERR, 0, "calloc");
    }

    if (body != NULL) {
        if (FIRSTCHILD(body) != NULL && FIRSTCHILD(body)->label == (label_t)DecVars) {
            iterate_vars(FIRSTCHILD(FIRSTCHILD(body)), f.local_table);
            // if ()
            // infer_array_size(f.local_table, f.params, f.nb_params, body);
        }
    }
    return f;
}

/*
Fonction
    ├── Header
    │   ├── voidType
    │   ├── id main
    │   └── param
    │       └── type int
    │           └── idArray a
    └── body
        ├── Assign
        │   ├── id a
        │   └── Number 1
        ├── Assign
        │   ├── id b
        │   └── Number 2
        └── ReturnNode
            └── FunctionCall
                ├── id main
                └── Args
                    ├── id a
                    └── id b
 */

static void put_foncts(Node* function_node, HashTable* ht) {
    Node *Header = FIRSTCHILD(function_node), *body = SECONDCHILD(function_node);
    Node* id = SECONDCHILD(Header);
    Function f = create_function_decl(Header, body);
    ValueType v = create_value(TYPE_FUNCTION, 0);
    v.value.function_val = f;
    v.size = compute_function_size(&f);
    table_put(ht, id->value, v);
}

static void apply_to_children(Node* parent, void (*func)(Node*, HashTable*), HashTable* ht) {
    Node* current = FIRSTCHILD(parent);
    while (current != NULL) {
        func(current, ht);
        current = current->nextSibling;
    }
}

static void create_hash_table_sub(Node* root, HashTable* ht) {
    Node* current = root;
    while (current != NULL) {
        if (current->label == (label_t)DecVars)
            apply_to_children(current, put_vars_void, ht);
        else if (current->label == (label_t)DecFoncts)
            apply_to_children(current, put_foncts, ht);
        current = current->nextSibling;
    }
}

HashTable* create_hash_table(Node* prog_node) {
    HashTable* ht = table_init();
    create_hash_table_sub(FIRSTCHILD(prog_node), ht);
    return ht;
}