/* tree.c */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"
extern int lineno; /* from lexer */
extern int charno; /* from lexer */
int opti = 0;

static const char *StringFromLabel[] = {
    FOREACH_TOKEN(GENERATE_STRING)
    /* list all other node labels, if any */
    /* The list must coincide with the label_t enum in tree.h */
    /* To avoid listing them twice, see https://stackoverflow.com/a/10966395 */
};

Node *makeNode(label_t label) {
    Node *node = calloc(1, sizeof(Node));
    if (!node) {
        printf("Run out of memory\n");
        exit(1);
    }
    node->label = label;
    node->firstChild = node->nextSibling = NULL;
    node->lineno = lineno;
    node->charno = charno;
    // node->value[0] = '\0';
    return node;
}

void addSibling(Node *node, Node *sibling) {
    Node *curr = node;
    while (curr->nextSibling != NULL) {
        curr = curr->nextSibling;
    }
    curr->nextSibling = sibling;
}

void addChild(Node *parent, Node *child) {
    if (parent->firstChild == NULL) {
        parent->firstChild = child;
    } else {
        addSibling(parent->firstChild, child);
    }
}

void deleteTree(Node *node) {
    if (node->firstChild) deleteTree(node->firstChild);
    if (node->nextSibling) deleteTree(node->nextSibling);
    // node->firstChild = node->nextSibling = NULL;
    memset(node, 0, sizeof(Node));
    free(node);
}

void printTree(Node *node) {
    static bool rightmost[128];        // tells if node is rightmost sibling
    static int depth = 0;              // depth of current node
    for (int i = 1; i < depth; i++) {  // 2502 = vertical line
        printf(rightmost[i] ? "    " : "\u2502   ");
    }
    if (depth > 0) {  // 2514 = L form; 2500 = horizontal line; 251c = vertical line and right horiz
        printf(rightmost[depth] ? "\u2514\u2500\u2500 " : "\u251c\u2500\u2500 ");
    }
    printf("%s", StringFromLabel[node->label]);
    // if (node->type[0] != '\0') {
    //     printf(" %s", node->type);
    // }
    // if (node->name[0] != '\0') {
    //     printf(" %s", node->name);
    // }
    if (node->value[0] != '\0') {
        printf(" %s", node->value);
    }
    printf("\n");
    depth++;
    for (Node *child = node->firstChild; child != NULL; child = child->nextSibling) {
        rightmost[depth] = (child->nextSibling) ? false : true;
        printTree(child);
    }
    depth--;
}

// void updateType(Node *node) {
//     Node *child = node->firstChild;
//     while (child != NULL) {
//         strcpy(node->type, child->type);
//         child = child->nextSibling;
//     }
// }

int numberChildren(Node *node) {
    Node *child = FIRSTCHILD(node);
    if (!child) return 0;
    int count = 1;
    while ((child = child->nextSibling) != NULL) {
        count++;
    }
    return count;
}

static void reduce_opers(Node *tree) {
    if (tree == NULL) return;
    Node *left = FIRSTCHILD(tree), *right = NULL;
    if (left) right = SECONDCHILD(tree);

    // reduce_opers(left);
    // reduce_opers(right);
    switch (tree->label) {
        case AddSub:
        case DivStar: {
            int is1 = left->label == Number || left->label == Character;
            int is2 = right->label == Number || right->label == Character;
            if (is1 && is2) {
                int a = atoi(left->value), b = atoi(right->value);
                if (left->label == Character) a = left->value[0];
                if (right->label == Character) b = right->value[0];
                int res;
                if (tree->label == AddSub) {
                    if (tree->value[0] == '+')
                        res = a + b;
                    else
                        res = a - b;
                } else {
                    if (tree->value[0] == '*')
                        res = a * b;
                    else {
                        if (b == 0) {
                            printf("Error: Division by zero\n");
                            exit(1);
                        }
                        if (tree->value[0] == '/')
                            res = a / b;
                        else
                            res = a % b;
                    }
                }

                tree->label = Number;
                sprintf(tree->value, "%d", res);
                deleteTree(left);
                FIRSTCHILD(tree) = NULL;
                return;
            }
        } break;
        case UnaryOp: {
            if (left->label == Number) {
                int a = atoi(left->value);
                int res;
                if (tree->value[0] == '-')
                    res = -a;
                else
                    res = !a;
                tree->label = Number;
                sprintf(tree->value, "%d", res);
                deleteTree(left);
                FIRSTCHILD(tree) = NULL;
                return;
            }
        } break;

        case AndOp:
        case OrOp: {
            int is1 = left->label == Number || left->label == Character;
            int is2 = right->label == Number || right->label == Character;
            if (is1 && is2) {
                int a = atoi(left->value), b = atoi(right->value);
                if (left->label == Character) a = left->value[0];
                if (right->label == Character) b = right->value[0];
                int res;
                if (tree->label == AndOp)
                    res = a && b;
                else
                    res = a || b;
                tree->label = Number;
                sprintf(tree->value, "%d", res);
                deleteTree(left);
                FIRSTCHILD(tree) = NULL;
                return;
            }
        } break;

        default:
            break;
    }
}

static int remove_unreachable(Node *NodeIf) {
    if (NodeIf == NULL || (NodeIf->label != IfNode && NodeIf->label != WhileNode)) return 0;
    int res = 0;
    Node *cond = FIRSTCHILD(NodeIf), *stmt = SECONDCHILD(NodeIf), *else_stmt = NULL;
    if (stmt) else_stmt = THIRDCHILD(NodeIf);
    if (cond->label == Number) {
        int tmp = atoi(cond->value);
        if (tmp == 0) {
            if (else_stmt) {
                if (!FIRSTCHILD(else_stmt))
                    NodeIf->firstChild = NULL;
                else
                    *NodeIf = *FIRSTCHILD(else_stmt);
                FIRSTCHILD(else_stmt) = NULL;
            }
            deleteTree(cond);
        } else if (tmp >= 1) {
            Node *nxt = NodeIf->nextSibling;
            if (!FIRSTCHILD(stmt)) {
                if (nxt) {
                    *NodeIf = *nxt;
                    free(nxt);
                } else {
                    free(NodeIf);
                    res = 2;
                }
            } else {
                *NodeIf = *FIRSTCHILD(stmt);
                NodeIf->nextSibling = nxt;
                free(FIRSTCHILD(stmt));
                FIRSTCHILD(stmt) = NULL;
            }
            deleteTree(cond);
        }
        if (res == 0 && numberChildren(NodeIf) == 0) res = 1;
    }
    return res;
}

static int tmp(Node *tree) {
    reduce_opers(tree);
    if (!opti) return 0;
    return remove_unreachable(tree);
}

static int parcours(Node *tree, int (*f)(Node *)) {
    if (tree == NULL) return 0;
    int res = 0;

    if ((res = parcours(tree->firstChild, f))) {
        Node *tmp = FIRSTCHILD(tree) /* , *nxt = tree->nextSibling */;
        if (res == 1) {
            FIRSTCHILD(tree) = SECONDCHILD(tree);
            tmp->nextSibling = NULL;
            deleteTree(tmp);
            return 0;
        } else if (res == 2) {
            FIRSTCHILD(tree) = NULL;
            // *tree = *nxt;
            // free(nxt);
            // return 0;
        }
    }
    if ((res = parcours(tree->nextSibling, f))) {
        Node *tmp = tree->nextSibling;
        if (res == 1) {
            tree->nextSibling = tree->nextSibling->nextSibling;
            tmp->nextSibling = NULL;
            deleteTree(tmp);
            return 0;
        }
    }
    return f(tree);
}

void optimise_tree(Node *tree) {
    // reduce_opers(tree);
    parcours(tree, tmp);
}
