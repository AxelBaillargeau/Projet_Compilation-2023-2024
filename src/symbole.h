#ifndef SYMBOLE_H
#define SYMBOLE_H

#include <stdint.h>

#define SYMBOLE_TABLE_MAX_SIZE 1008
#define UNFOUND_VALUETYPE    \
    (ValueType) {            \
        -1, -1, TYPE_INT, {  \
            .array.size = ~0 \
        }                    \
    }

typedef enum _type { TYPE_VOID, TYPE_INT, TYPE_CHAR, TYPE_FUNCTION, TYPE_INT_ARRAY, TYPE_CHAR_ARRAY } Type;

typedef struct _function {
    Type return_type;
    uint64_t nb_params;
    struct _valueType *params;
    struct _table *local_table;
} Function;

typedef union _value {
    Function function_val;
    struct {
        uint64_t size;
    } array;
} Value;

typedef struct _valueType {
    uint64_t address;
    uint64_t size;
    Type type;
    Value value;
} ValueType;

typedef struct _entry {
    char *identifier;
    ValueType value;
    struct _entry *next;
} Entry;

typedef struct _table {
    Entry *buckets[SYMBOLE_TABLE_MAX_SIZE];
    uint64_t size;
} Table, HashTable;

/**
 * @brief Hold the symbol declaration during the compilation.
 *
 */
typedef struct table_context {
    HashTable *global;
    HashTable *local;
} Context;

static const uint16_t k_hash = 613;
static const uint16_t N_hash = SYMBOLE_TABLE_MAX_SIZE;

/**
 * @brief Renvoie le hash d'une chaine de caractères
 *
 * @param key
 * @return uint64_t
 */
uint64_t hash(char *key);

int isUnfound(ValueType value);

/**
 * @brief Initialise une table de symboles
 *
 * @return HashTable*
 */
HashTable *table_init();

/**
 * @brief Libère la mémoire allouée pour une table de symboles
 *
 * @param t
 */
void table_destroy(HashTable *t);

/**
 * @brief Rentre une valeur dans la table de symboles. Si une collision est détectée, affiche un message d'erreur et
 * l'ajoute au début de la liste chainée.
 *
 * @param table
 * @param key
 * @param value
 * @return int
 */
int table_put(HashTable *table, char *key, ValueType value);

/**
 * @brief Récupère une valeur dans la table de symboles. Renvoie UNFOUND_VALUETYPE si la valeur n'est pas trouvée
 *
 * @param table
 * @param key
 * @return ValueType
 */
ValueType table_get(HashTable *t, char *key);

Entry *table_get_adr(HashTable *table, char *key);

/**
 * @brief Supprime une valeur dans la table de symboles. Renvoie 0 si la valeur a été supprimée, 1 sinon
 *
 * @param t
 * @param key
 * @return int
 */
int table_remove(HashTable *t, char *key);

/**
 * @brief Renvoie le type correspondant à une chaine de caractères
 *
 * @param type
 * @return Type
 */
Type type_from_string(char *type);

/**
 * @brief Crée une valeur à partir d'un type et d'une taille de mémoire. Si la taille de mémoire est >0, renvoie une
 * taille de mémoire multpliée par la taille du type
 *
 * @param type
 * @param memorySize
 * @return ValueType
 */
ValueType create_value(Type type, uint64_t memorySize);

HashTable *get_function_table(ValueType *function);

/**
 * @brief Imprime les valeurs contenues dans la table de symboles. Puisque la table de symboles est un tableau de
 * hashage, les valeurs ne sont pas imprimées dans l'ordre dans lequel elles ont été ajoutées mais dans l'ordre des
 * index du tableau.
 *
 * @param table
 */
void print_table(HashTable *table);

void print_value_type(ValueType value);

int compute_function_size(Function *function);

Function create_function(char *returnValue, char *id);

void add_param(Function *f, Type t);

ValueType search_var(Context cont, char *name, int *type);
int size_of(Type type);
#endif