/* tree.h */

// TODO METTRE EN MAJUSCULES
#define FOREACH_TOKEN(TOKEN) \
    TOKEN(PROG)              \
    TOKEN(DecVars)           \
    TOKEN(DecFoncts)         \
    TOKEN(idType)            \
    TOKEN(type)              \
    TOKEN(id)                \
    TOKEN(idArray)           \
    TOKEN(Number)            \
    TOKEN(Character)         \
    TOKEN(Fonction)          \
    TOKEN(Param)             \
    TOKEN(Body)              \
    TOKEN(Assign)            \
    TOKEN(condition)         \
    TOKEN(ReturnNode)        \
    TOKEN(Args)              \
    TOKEN(OrOp)              \
    TOKEN(AndOp)             \
    TOKEN(EqOp)              \
    TOKEN(Order)             \
    TOKEN(AddSub)            \
    TOKEN(DivStar)           \
    TOKEN(UnaryOp)           \
    TOKEN(FunctionCall)      \
    TOKEN(WhileNode)         \
    TOKEN(IfNode)            \
    TOKEN(StructNode)        \
    TOKEN(Instr)             \
    TOKEN(Header)            \
    TOKEN(size)              \
    TOKEN(voidType)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum _label_t {
    FOREACH_TOKEN(GENERATE_ENUM)
    /* list all other node labels, if any */
    /* The list must coincide with the string array in tree.c */
    /* To avoid listing them twice, see https://stackoverflow.com/a/10966395 */
} label_t;

typedef struct Node {
    label_t label;
    struct Node *firstChild, *nextSibling;
    int lineno;
    int charno;
    char value[64];
} Node;

Node *makeNode(label_t label);
void addSibling(Node *node, Node *sibling);
void addChild(Node *parent, Node *child);
void deleteTree(Node *node);
void printTree(Node *node);
void addString(Node *node, char *string);
void updateType(Node *node);
int numberChildren(Node *node);

#define FIRSTCHILD(node) node->firstChild
#define SECONDCHILD(node) node->firstChild->nextSibling
#define THIRDCHILD(node) node->firstChild->nextSibling->nextSibling

void optimise_tree(Node *tree);