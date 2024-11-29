#include "symbole.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <error.h>
#include "tree_value.h"  // for error values

int dry_run = 0;  // extern in tpc-bison.y for dry-run mode

static const u_char sizes[6] = {0, 4, 1, 0, 8, 8};  // size of int, char, function, int_array, char_array, void
static const char* type_strings[6] = {"void", "int", "char", "function", "int_array", "char_array"};

int size_of(Type type) {
    if (type == ~-0) return 0;
    return sizes[type];
}

uint64_t hash(char* key) {
    uint64_t h = 0;
    for (int i = 0; key[i] != '\0'; i++) h = (k_hash * h + key[i]) % N_hash;
    return h;
}

HashTable* table_init() {
    HashTable* table = calloc(1, sizeof(*table));
    if (!table) error(MISC_ERR, 0, "calloc");
    // for (int i = 0; i < SYMBOLE_TABLE_MAX_SIZE; i++) table->buckets[i] = NULL;
    return table;
};

static void freeEntry(Entry* entry) {
    free(entry->identifier);
    ValueType tmp = entry->value;
    if (tmp.type == TYPE_FUNCTION) {
        table_destroy(entry->value.value.function_val.local_table);
        free(tmp.value.function_val.params);
        entry->value.value.function_val.local_table = tmp.value.function_val.local_table = NULL;
    }
    free(entry);
}

int ValueEqual(ValueType x, ValueType y) { return x.address == y.address && x.size == y.size; }

int isUnfound(ValueType value) { return ValueEqual(UNFOUND_VALUETYPE, value); }

void table_destroy(HashTable* t) {
    if (t == NULL) return;
    for (int i = 0; i < SYMBOLE_TABLE_MAX_SIZE; i++) {
        Entry* entry = t->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            freeEntry(entry);
            entry = next;
        }
    }
    free(t);
}

// static int check_duplicate(HashTable* table, unsigned int index) {
//     Entry* entry = table->buckets[index];
//     Entry* pivot = entry;
//     while (pivot) {
//         entry = pivot->next;
//         while (entry) {
//             if (strcmp(entry->identifier, pivot->identifier) == 0) {
//                 return 1;
//             }
//             entry = entry->next;
//         }
//         pivot = pivot->next;
//     }

//     return 0;
// }

static int check_duplicate2(HashTable* table, unsigned int index, char* key) {
    Entry* entry = table->buckets[index];
    while (entry) {
        if (strcmp(entry->identifier, key) == 0) {
            return 1;
        }
        entry = entry->next;
    }
    return 0;
}

int table_put(HashTable* table, char* key, ValueType value) {
    unsigned int index = hash(key);
    if (table->buckets[index] != NULL) {
        if (!dry_run && check_duplicate2(table, index, key)) error(SEM_ERR, 0, "Duplicate key: %s", key);
    }

    Entry* entry = calloc(1, sizeof(*entry));
    if (entry == NULL) err_prnt(MISC_ERR, 0, 0, "calloc");

    entry->identifier = strdup(key);  // id belongs to the tree, we copy it in case tree is freed first
    if (entry->identifier == NULL) err_prnt(MISC_ERR, 0, 0, "strdup");

    if (value.type != TYPE_FUNCTION) {
        value.address = table->size;
        table->size += value.size;
    }
    if (value.type >= TYPE_INT_ARRAY) table->size += size_of(value.type - 3) * value.value.array.size;

    entry->value = value;
    entry->next = table->buckets[index];
    table->buckets[index] = entry;  // we add the new entry at the beginning of the list
    return 0;
}

Entry* table_get_adr(HashTable* table, char* key) {
    unsigned int index = hash(key);
    if (table == NULL) return NULL;
    Entry* entry = table->buckets[index];
    while (entry) {
        if (strcmp(entry->identifier, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;  // if not found
}

ValueType table_get(HashTable* table, char* key) {
    Entry* entry = table_get_adr(table, key);
    if (entry) return entry->value;
    return UNFOUND_VALUETYPE;
}

int table_remove(HashTable* t, char* key) {
    unsigned int index = hash(key);
    Entry* entry = t->buckets[index];
    Entry* prev = NULL;
    while (entry) {
        if (strcmp(entry->identifier, key) == 0) {
            if (prev == NULL)
                t->buckets[index] = entry->next;
            else
                prev->next = entry->next;

            freeEntry(entry);
            return 0;
        }
        prev = entry;
        entry = entry->next;
    }
    return 1;
}

Type type_from_string(char* type) {
    if (strcmp(type, "int") == 0) return (Type)TYPE_INT;
    if (strcmp(type, "char") == 0) return (Type)TYPE_CHAR;
    if (type[0] == '\0') return (Type)TYPE_VOID;
    return -1;
}

ValueType create_value(Type type, uint64_t memorySize) {
    ValueType v = UNFOUND_VALUETYPE;
    v.type = type;
    v.address = 0;
    if (type == TYPE_FUNCTION) {
        v.size = 0;
        v.value.function_val.return_type = TYPE_VOID;
        v.value.function_val.nb_params = 0;
    } else
        v.size = size_of(type);
    if (memorySize > 0) {
        v.value.array.size = memorySize;
        v.type += 3;
        v.size = size_of(v.type);
    }
    return v;
}

HashTable* get_function_table(ValueType* function) { return function->value.function_val.local_table; }

void print_value_type(ValueType value) {
    printf("size: %ld, address: %ld, type: ", value.size, value.address);
    switch (value.type) {
        case TYPE_INT:
            printf("int");
            break;
        case TYPE_CHAR:
            printf("char");
            break;
        case TYPE_FUNCTION:
            printf("function, return type: %s, nb args: %ld\n", type_strings[value.value.function_val.return_type],
                   value.value.function_val.nb_params);
            for (int i = 0; i < value.value.function_val.nb_params; i++)
                printf("\targ %d: %s\n", i, type_strings[value.value.function_val.params[i].type]);
            break;
        case TYPE_INT_ARRAY:
        case TYPE_CHAR_ARRAY:
            if (value.type == TYPE_INT_ARRAY)
                printf("int");
            else
                printf("char");
            printf(" Array, capacity: %ld", value.value.array.size);
            break;
        default:
            printf("Unknown type");
            break;
    }
}

static void print_entry(Entry entry) {
    static const char* stdfunc[] = {"putint", "putchar", "getint", "getchar"};
    char* ident = entry.identifier;
    if (entry.value.type == TYPE_FUNCTION) {
        for (int i = 0; i < 4; i++)
            if (strcmp(ident, stdfunc[i]) == 0) return;
        printf("function\n");
    }
    printf("key: %s, ", ident);
    print_value_type(entry.value);
    if (entry.value.type == TYPE_FUNCTION) print_table(entry.value.value.function_val.local_table);
}

void print_table(HashTable* table) {
    if (table == NULL) return;
    for (int i = 0; i < SYMBOLE_TABLE_MAX_SIZE; i++) {
        Entry* entry = table->buckets[i];
        while (entry) {
            print_entry(*entry);
            printf("\n");
            entry = entry->next;
        }
    }
}

static int isArray(ValueType value) { return value.type == TYPE_INT_ARRAY || value.type == TYPE_CHAR_ARRAY; }

int compute_function_size(Function* function) {
    uint64_t size = 0;
    Entry* curr;
    for (int i = 0; i < SYMBOLE_TABLE_MAX_SIZE; i++) {
        curr = function->local_table->buckets[i];
        while (curr) {
            size += curr->value.size;
            if (isArray(curr->value)) size += size_of(curr->value.type - 3) * curr->value.value.array.size;
            curr = curr->next;
        }
    }
    return size;
}

Function create_function(char* returnValue, char* id) {
    Function f;
    f.return_type = type_from_string(returnValue);
    f.nb_params = 0;
    f.params = NULL;
    f.local_table = NULL;
    return f;
}

void add_param(Function* f, Type t) {
    if (f->params == NULL) {
        f->params = calloc(1, sizeof(*f->params));
        if (f->params == NULL) error(MISC_ERR, 0, "calloc");
    } else {
        f->params = realloc(f->params, (f->nb_params + 1) * sizeof(*f->params));
        if (f->params == NULL) error(MISC_ERR, 0, "realloc");
    }
    f->params[f->nb_params].type = t;
    f->params[f->nb_params].address = 0;
    f->params[f->nb_params].size = sizes[t];
    f->nb_params++;
}

ValueType search_var(Context cont, char* name, int* glb) {
    ValueType value = table_get(cont.local, name);
    *glb = 0;
    if (isUnfound(value)) {
        *glb = 1;
        value = table_get(cont.global, name);
        if (isUnfound(value)) *glb = 2;
    }
    return value;
}
