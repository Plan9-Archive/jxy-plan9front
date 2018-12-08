void yyerror(char *);
int yylex(void);
int yyparse(void);
Node *node(int, ...);
int nodetfmt(Fmt *);
int typetfmt(Fmt *);
int typefmt(Fmt *);
int nodefmt(Fmt *);
void *emalloc(ulong);
void *erealloc(void *, ulong);
DTAct action(int, DTExpr *);
DTExpr *codegen(Node *);
void error(char *, ...);
Symbol *getsym(char *);
void lexinit(void);
void lexstring(char *);
void clausebegin(void);
void addstat(int, ...);
void addarg(Node *);
void addprobe(char *);
void addpred(DTExpr *);
void clauseend(void);
void packclauses(Fmt *);
void addepid(u32int, u32int, int, char*);
int parsebuf(uchar *, int, Biobuf*);
Node *tracegen(Node *, DTActGr *, int *);
void actgradd(DTActGr *, DTAct);
void needruntime(Node *);
void dump(void);
vlong evalop(int, int, vlong, vlong);
Node *exprcheck(Node *, int);
Type *type(int, ...);
int min(int, int);
int max(int, int);
Node *addtype(Type *, Node *);
int aggparsebuf(uchar *, int);
int aggnote(void *, char *);
void aggdump(void);
void agginit(void);
