#ifndef ASM_WRITER_H
#define ASM_WRITER_H

#include <stdio.h>
#include "symbole.h"
#include "tree.h"
#include "errors.h"

void asm_gen(Node *root, HashTable *symbols, FILE *file);

#endif