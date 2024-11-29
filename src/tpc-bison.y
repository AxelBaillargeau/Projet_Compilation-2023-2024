%code top {
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <err.h>
    #include <getopt.h>
}

%code requires{
    #include "../src/tree_value.h"
}

%code {
    extern int lineno;
    extern int charno;
    extern char lastWord[256];
    extern int readFile(char* filename);
    extern int dry_run;
    extern int opti;
    extern int debug;
    static void error_template(char* message);
    int yyerror(char *msg);
    int yylex();
    int yyparse(void);
    Node* root;
    int tree = 0, read = 0, symbol = 0;
    int returnVal = PARSE_SUCCESS;
    char* file = NULL;
    HashTable* main_symboleTable = NULL;
    ValueType current_value = UNFOUND_VALUETYPE;
    Type current_type = -1;
    char ident[64];
    char out_str[256] = {0};
}

%union {
    char byte;
    int num;
    char ident[64];
    char comp[3];
    char type[6];
    struct Node* node;
}

%token <byte> CHARACTER ADDSUB DIVSTAR
%token <num> NUM
%token <ident> IDENT
%token <comp> ORDER EQ
%token <type> TYPE
%token OR AND IF ELSE WHILE RETURN VOID
%type <node> DeclVars Declarateurs DeclFoncts DeclFonctsSub DeclFonct EnTeteFonct Parametres ListTypVar TypVar Body Interm
%type <node> SuiteInstr Function Instr LValue Arguments ListExp Exp TB FB M E T F IfBlocks TypeDecl IdentVal 
%type <node> VoidType

%right ELSE

%%

Prog:   
    DeclVars DeclFoncts {
            if ($1 != NULL) addChild(root, $1);
            if ($2 != NULL) addChild(root, $2);
            return returnVal;
        }
    ;
DeclVars:
        DeclVars TypeDecl Declarateurs ';'  {
                if ($1 != NULL) $$ = $1;
                else {
                    $$ = makeNode((label_t)DecVars);
                    $$->lineno = lineno;
                    if($$ == NULL) {
                        fprintf(stderr, "Error allocating memory\n");
                        returnVal = MISC_ERR;
                    }
                    }
                struct Node* type = $2;
                
                addChild($$, type);
                addChild(type, $3);
            }
    |   %empty {$$ = NULL;}
    ;
Declarateurs:
        Declarateurs ',' Interm { 
                $$ = $1;
                addSibling($$, $3);
            }
    |   Interm {
                $$ = $1;           
        }
    ;
Interm: 
        IdentVal {
            $$ = $1;
            }
    |   IdentVal '[' NUM ']' {
            $$ = $1;
            struct Node* tmp = makeNode((label_t)size);
            $$->lineno = lineno;
            if(tmp == NULL){
                fprintf(stderr,"Error allocating memoy");
                returnVal = MISC_ERR;
            }
            addChild($$, tmp);
            if ($3 <= 0) {
                error_template("Array size must be greater than 0\n");
                returnVal = MISC_ERR;
            }
            sprintf($$->firstChild->value, "%d", $3);
        }
    ;

DeclFoncts:
        DeclFonctsSub {
            $$ = makeNode((label_t)DecFoncts);
            $$->lineno = lineno;
            if ($$ == NULL) {
                fprintf(stderr, "Error allocating memory\n");
                returnVal = MISC_ERR;
            }
            addChild($$, $1);
            }
    ;
DeclFonctsSub:
        DeclFonct DeclFonctsSub {
            $$ = $2;
            addSibling($$, $1);
            }
    |   DeclFonct {  
            $$ = $1;
            }  
    ;
DeclFonct:
        EnTeteFonct Body {
                $$ = makeNode((label_t)Fonction);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $1);
                if ($2 != NULL) addChild($$, $2);
            }
    ;
EnTeteFonct:
        TypeDecl IdentVal '(' Parametres ')' {
            $$ = makeNode((label_t)Header);
            $$->lineno = lineno;
            if($$ == NULL) {
                fprintf(stderr, "Error allocating memory\n");
                returnVal = MISC_ERR;
            }
            addChild($$, $1);
            addChild($$, $2);
            addChild($$, $4);
            }
    |   VoidType IdentVal '(' Parametres ')'{
            $$ = makeNode((label_t)Header);
            $$->lineno = lineno;
            if($$ == NULL) {
                fprintf(stderr, "Error allocating memory\n");
                returnVal = MISC_ERR;
            }

            Node*type = $1;

            addChild($$,type);
            addChild($$,$2);
            addChild($$, $4);
            }
    ;
Parametres:
        VOID {
            $$ = makeNode((label_t)Param);
            $$->lineno = lineno;
            if($$ == NULL) {
                fprintf(stderr, "Error allocating memory\n");
                returnVal = MISC_ERR;
            }
            Node*type = makeNode((label_t)voidType);
            $$->lineno = lineno;
            if(type == NULL) {
                fprintf(stderr, "Error allocating memory\n");
                returnVal = MISC_ERR;
            }
            addChild($$,type);
            }
    |   ListTypVar {
            $$ = makeNode((label_t)Param);
            $$->lineno = lineno;
            if($$ == NULL) {
                fprintf(stderr, "Error allocating memory\n");
                returnVal = MISC_ERR;
            }
            addChild($$,$1);
            }
    ;
ListTypVar:
        ListTypVar ',' TypVar {
                $$ = $1;
                addSibling($$, $3);
            }
    |   TypVar {$$ = $1;}
    ;
TypVar:
        TypeDecl IdentVal {
                $$ = $1;
                addChild($$, $2);
            }
    |   TypeDecl IdentVal '[' ']' {
                $$ = $1;
                $2->label = (label_t)idArray;
                addChild($$, $2);
            }
    ;
Body: 
        '{' DeclVars SuiteInstr '}' {
                if ($2 != NULL || $3 != NULL) {
                    $$ = makeNode((label_t)Body);
                    $$->lineno = lineno;
                    if($$ == NULL) {
                        fprintf(stderr, "Error allocating memory\n");
                        returnVal = MISC_ERR;
                    }
                    if ($2 != NULL) addChild($$, $2);
                    if ($3 != NULL) addChild($$, $3);}
                else $$ = NULL;
            }
    ;
SuiteInstr:
        SuiteInstr Instr {
                if ($1 != NULL) 
                    $$ = $1;
                else $$ = $2;
                if ($2 != NULL && $$ != $2)
                    addSibling($$, $2);
            }
    |   %empty {
            $$ = NULL;
            }
    ;
Instr:
        LValue '=' Exp ';' {
            
                $$ = makeNode((label_t)Assign);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $1);
                addChild($$, $3);
            }
    |   IfBlocks {
            
                $$ = $1;
            
            }
    |   WHILE '(' Exp ')' Instr {
                $$ = makeNode((label_t)WhileNode);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $3);

                if ($5 != NULL) {
                    struct Node* tmp1 = makeNode((label_t)Instr);
                    $$->lineno = lineno;
                    if(tmp1 == NULL){
                        fprintf(stderr,"Error allocating memoy");
                        returnVal = MISC_ERR;
                    }

                    addChild(tmp1, $5);
                    addChild($$, tmp1);
                }
            
        }
    |   Function ';' {
            
                $$ = $1;
            
        }
    |   RETURN Exp ';' {
            
                $$ = makeNode((label_t)ReturnNode);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $2);
            
        }
    |   RETURN ';' {            
                $$ = makeNode((label_t)ReturnNode);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
        }
    |   '{' SuiteInstr '}' {
            
                $$ = $2;
            
        }
    |   ';' {
            $$ = NULL;
        }
    ;

Exp:  
    Exp OR TB {
            
                $$ = makeNode((label_t)OrOp);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                strcpy($$->value, "||");
                addChild($$, $1);
                addChild($$, $3);
            }
    |   TB {
                $$ = $1;
            }
    ;
TB:  
    TB AND FB {
                $$ = makeNode((label_t)AndOp);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                strcpy($$->value, "&&");
                addChild($$, $1);
                addChild($$, $3);
            }
    |   FB  {
                $$ = $1;
            }
    ;
FB: 
    FB EQ M {
                $$ = makeNode((label_t)EqOp);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                strcpy($$->value, "==");
                if ($2[0] == '!') $$->value[0] = '!';
                addChild($$, $1);
                addChild($$, $3);
            }
    |   M {
                $$ = $1;
            }
    ;
M:  
    M ORDER E {
                $$ = makeNode((label_t)Order);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                $$->value[0] = $2[0]; $$->value[1] = $2[1]; $$->value[2] = '\0';


                addChild($$, $1);
                addChild($$, $3);
            }
    |   E {
                $$ = $1;
            }
    ;
E:  
    E ADDSUB T {
                $$ = makeNode((label_t)AddSub);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                $$->value[0] = $2; $$->value[1] = '\0';
                addChild($$, $1);
                addChild($$, $3);
            }
    |   T   {
                $$ = $1;
            }
    ;
T:  
    T DIVSTAR F  {
                $$ = makeNode((label_t)DivStar);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                $$->value[0] = $2; $$->value[1] = '\0';
                addChild($$, $1);
                addChild($$, $3);
            }
    |   F  {
                $$ = $1;
            }
    ;
F:  
    ADDSUB F {
                $$ = makeNode((label_t)UnaryOp);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                $$->value[0] = $1; $$->value[1] = '\0';
                addChild($$, $2);
            }
    |  '!' F {
            
                $$ = makeNode((label_t)UnaryOp);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                strcpy($$->value, "!");
                addChild($$, $2);
            }
    |  '(' Exp ')' {
            
                $$ = $2;
            }
    |   NUM  {
            
                $$ = makeNode((label_t)Number);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                sprintf($$->value, "%d", $1);
            }
    |   CHARACTER {
            
                $$ = makeNode((label_t)Character);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                sprintf($$->value, "%c", $1);
            }
    |   LValue  {
            
                $$ = $1;
            }
    |   Function {$$ = $1;}
    |  '{' ListExp '}' {
            
                $$ = makeNode((label_t)StructNode);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $2);
            }
    ;
Function: 
        IdentVal '(' Arguments ')' {
                $$ = makeNode((label_t)FunctionCall);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                addChild($$, $1);
                addChild($$, $3);
            }
    ;
LValue:
        IdentVal {$$ = $1;}
    |   IdentVal '[' Exp ']' {   
                $$ = makeNode((label_t)idArray);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                strcpy($$->value, $1->value);
                addChild($$, $3);
    }
    ;
Arguments:
       ListExp { {
                $$ = makeNode((label_t)Args);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $1);
       }}
    |  %empty { $$ = NULL;}
    ;
ListExp:
    Exp ',' ListExp {
                $$ = $1;
                addSibling($$, $3);
            }
    | Exp {$$ = $1;}


IfBlocks: 
    IF '(' Exp ')' Instr %prec ELSE{
                $$ = makeNode((label_t)IfNode);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $3);
                struct Node* tmp1 = makeNode((label_t)Instr);
                $$->lineno = lineno;
                if(tmp1 == NULL){
                    fprintf(stderr,"Error allocating memoy");
                    returnVal = MISC_ERR;
                }
                addChild($$, tmp1);
                if ($5 != NULL) addChild(tmp1, $5);
        }
    |   IF '(' Exp ')' Instr ELSE Instr {
                $$ = makeNode((label_t)IfNode);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                
                addChild($$, $3);
                struct Node* stmt = makeNode((label_t)Instr);
                $$->lineno = lineno;
                if(stmt == NULL){
                    fprintf(stderr,"Error allocating memoy");
                    returnVal = MISC_ERR;
                }
                addChild($$, stmt);
                if ($5 != NULL) addChild(stmt, $5);
                
                struct Node* estmt = makeNode((label_t)Instr);
                $$->lineno = lineno;
                if(estmt == NULL){
                    fprintf(stderr,"Error allocating memoy");
                    returnVal = MISC_ERR;
                }
                addChild($$, estmt);
                if ($7 != NULL) addChild(estmt, $7);
        }


TypeDecl: 
        TYPE {
                $$ = makeNode((label_t)type);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
                strcpy($$->value, $1);
            }
    ;

VoidType: 
        VOID {
                $$ = makeNode((label_t)voidType);
                $$->lineno = lineno;
                if($$ == NULL) returnVal = MISC_ERR;
            }
    ;

IdentVal:
        IDENT {
            $$ = makeNode((label_t)id);
            $$->lineno = lineno;
            if($$ == NULL) returnVal = MISC_ERR;
            strcpy($$->value, $1);
            }
%%
static void error_template(char* message){
    fprintf(stderr, "Syntaxical error line %d %d: %s\n", lineno, charno, message);
}

static void help(char *exec_name){
    printf("Usage: %s [options]\n", exec_name);
    printf("Options:\n");
    printf("  -t, --tree\t\tPrint the tree\n");
    printf("  -h, --help\t\tPrint this help\n");
    printf("  -f, --file <file>\tRead from file\n");
    printf("  -d, --dry\t\tDo not perform analysis\n");
    printf("  -s, --symtabs\t\tPrint the symbol table\n");
    printf("  -O, --no-opti\t\tDo not perform optimisation\n");
    printf("  -g, --debug\t\tPrint debug comments\n");
    printf("  -o, --output <file>\tOutput file\n");
}

static int options(char *s[], int n){
    static struct option long_options[] = {
        {"tree", no_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {"dry", no_argument, 0, 'd'},
        {"file", required_argument, 0, 'f'},
        {"symtabs", no_argument, 0, 's'},
        {"no-opti", no_argument, 0, 'O'},
        {"debug", no_argument, 0, 'g'},
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };

    int opt = '?', option_index = 0, cnt = 0;
    while ((opt = getopt_long(n, s, "thdsOgf:o:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 't':
                tree = 1;
                break;
            case 'h':
                help(s[0]);
                return 0;
            case 'd':
                dry_run = 1;
                break;
            case 'f':
                if (read > 0) {
                    fprintf(stderr, "Too many files\n");
                    free(file);
                    return 2;
                }
                if (optarg == NULL) {
                    fprintf(stderr, "No file specified\n");
                    return 2;
                }
                printf("file: %s\n", optarg);
                file = calloc(strlen(optarg), sizeof(char));
                if (file == NULL) return(MISC_ERR);
                strcpy(file, optarg);
                read++;
                break;
            case 's':
                symbol = 1;
                break;
            case 'O':
                opti = 1;
                break;
            case 'g':
                debug = 1;
                break;
            case 'o':
                if (optarg == NULL) {
                    fprintf(stderr, "No output file specified\n");
                    return 2;
                }
                strcpy(out_str, optarg);
                break;
            default:
                fprintf(stderr, "Unknown option: %c\n", optopt);
                return MISC_ERR;
        }
        cnt++;
    }
    int i = n - 1;
    if (i > cnt) strcpy(out_str, s[i]);
    
    return 1;
}

int yyerror(char * s){
    fprintf(stderr, "%s on line %d %d: unexpected token '%s'\n",s,lineno,charno,lastWord);
    return 0;
}

int main(int argc, char **argv) {
    FILE * out;
    if (argc > 1) {
        int res = options(argv, argc);
        if (res == 0) return PARSE_SUCCESS;
        if (res == MISC_ERR) return MISC_ERR;
        if (read != 0) {
            int res = readFile(file);
            if (res == 2) return MISC_ERR;
        }
    }
    if (out_str[0] == '\0') strcpy(out_str, "./_anonymous.asm");

    root = makeNode((label_t)PROG);
    if(root == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        return MISC_ERR;
    }
    int tmp = yyparse();
    
    if (tmp == PARSE_SUCCESS) {
        optimise_tree(root);
        if (tree) printTree(root);
        if (!dry_run) {
            main_symboleTable = create_hash_table(root);
            out = fopen(out_str, "w+");            
            asm_gen(root, main_symboleTable, out);
            if (symbol) print_table(main_symboleTable);
            fclose(out);
            table_destroy(main_symboleTable);
        }
    }
    
    if (file != NULL) free(file);
    deleteTree(root);
    
    return tmp;
}
