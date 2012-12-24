
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEFAULT "(& (>> t 6) (& (* 2 t) (>> t 1)))"
# define NUMNODE 160

struct node {
    char type;
    char cval;
    unsigned int ival;
    struct node *lval;
    struct node *rval;
};

void print_sexpr(struct node *sexpr);
unsigned int execute(struct node *sexpr, unsigned int t);
void play(struct node *sexpr);
int find_split(char* s, int start, int end);
struct node *parse(char *s, int start, int end);
struct node *new_node(char type, char cval, unsigned int ival,
                      struct node *lval, struct node *rval);
void test();

static struct node node_table[NUMNODE];
static int newest_node = 0;

char inbuffer[256];

int sstrlen(char *s, int max);

int sstrlen(char *s, int max) {
    int i;
    for (i=0; i<max; i++) {
        if (s[i] == '\n' || s[i] == '\0') {
            return i+1;
        }
    }
    return max;
}

void main() {
    int len;
    //test();
    if (stderr == stdout) {
        printf("You are expected to send stderr and stdout different ways.\n");
        exit(-1);
    }
    while (1) {
        fprintf(stderr, "Input a tune in exact s-expr syntax:\n");
        fprintf(stderr, "> ");
        fgets(inbuffer, 256, stdin);    
        len = sstrlen(inbuffer, 256);
        if (len == 256 || len < 1)
            fprintf(stderr, "Invalid input!\n");
        else
            break;
    }
    play(parse(inbuffer, 0, len-2));
    //play(parse(DEFAULT, 0, strlen(DEFAULT)-1));
};

void test() {
    printf(DEFAULT);
    printf("\n");
    print_sexpr(parse(DEFAULT, 0, strlen(DEFAULT)-1));
    printf("\n");
}

int digtoi(char *s, int start, int end) {
    int num = 0;
    for(;start<=end;start++) {
        if (! isdigit(s[start])) {
            fprintf(stderr, "parse: not a digit: %c", s[start]);
            exit(-1);
        }
        num = num*10 + (s[start] - '0');
    }
    return num;
}

struct node *parse(char *s, int start, int end) {
    struct node *lval, *rval;
    char cval;
    int offset, split;

    // number
    if (isdigit(s[start])) {
        return new_node('n', '_', digtoi(s, start, end), NULL, NULL);
    }
    // variable
    if ((s[start] == 't') && (end-start == 0)) {
        return new_node('v', '_', 0, NULL, NULL);
    }

    // must be an s-exp
    if ( !(s[start] == '(' && s[end] == ')') || (end-start < 4)) {
        fprintf(stderr, "parse: unparsable: ");
        for (;start<=end;start++) {
            fprintf(stderr, "%c", s[start]);
        }
        fprintf(stderr, "\n");
        exit(-1);
    }

    // unitary operator
    if (s[start+1] == '~') {
        rval = parse(s, start+3, end-1);
        return new_node('u', '~', 0, NULL, rval);
    }

    // various binary operators
    if ( strchr("*/%+-&^|<>%", s[start+1]) != NULL) {
        cval = s[start+1];
        offset = 3;
        if (cval == '<' && s[start+2] == '<' && s[start+3] == ' ') {
            cval = 'l';
            offset = 4;
        } else if (cval == '>' && s[start+2] == '>' && s[start+3] == ' ') {
            cval = 'r';
            offset = 4;
        } else if (s[start+2] != ' ') {
            fprintf(stderr, "parse: invalid operator: %c\n", cval);
            exit(-1);
        }
        split = find_split(s, start+offset, end-1);
        lval = parse(s, start+offset, split-1);
        rval = parse(s, split+1, end-1);
        return new_node('b', cval, 0, lval, rval);
    }

    // should not get here 
    fprintf(stderr, "parse: feel through\n");
    exit(-1);
}

unsigned int execute(struct node *sexpr, unsigned int t) {
    switch (sexpr->type) {
    // atom
    case 'v':
        return t;
    case 'n':
        return sexpr->ival;
    // unary
    case 'u':
        if (sexpr->cval == '~') {
            return (~ execute(sexpr->rval, t));
        } else {
            fprintf(stderr, "unexpected unary oper: %c",  sexpr->cval);
        }
    // binary
    case 'b':
        switch (sexpr->cval) {
        case '+':
            return execute(sexpr->lval, t) + execute(sexpr->rval, t);
        case '-':
            return execute(sexpr->lval, t) - execute(sexpr->rval, t);
        case '*':
            return execute(sexpr->lval, t) * execute(sexpr->rval, t);
        case '/':
            return execute(sexpr->lval, t) / execute(sexpr->rval, t);
        case '^':
            return execute(sexpr->lval, t) ^ execute(sexpr->rval, t);
        case '&':
            return execute(sexpr->lval, t) & execute(sexpr->rval, t);
        case '|':
            return execute(sexpr->lval, t) | execute(sexpr->rval, t);
        case '%':
            return execute(sexpr->lval, t) % execute(sexpr->rval, t);
        case 'r':
            return execute(sexpr->lval, t) >> execute(sexpr->rval, t);
        case 'l':
            return execute(sexpr->lval, t) << execute(sexpr->rval, t);
        default:
            fprintf(stderr, "unexpected binary oper: %c", sexpr->cval);
            exit(-1);
        }
    default:
        fprintf(stderr, "execute: unknown type: %c", sexpr->type);
        exit(-1);
    }
}

void play(struct node *sexpr) {
    unsigned int t = 0;
    for (;;t++) {
        printf("%c", (char)(0x000000FF & execute(sexpr, t)));
    }
    return;
}

int find_split(char *s, int start, int end) {
    int depth = 0;
    int i;
    for (i=start; i <= end; i++) {
        if ((depth == 0) && (s[i] == ' '))
            return i;
        if (s[i] == '(')
            depth++;
        if (s[i] == ')')
            depth--; 
        if (depth < 0) {
            fprintf(stderr, "parse: unmatched ')'\n");
            exit(-1);
        }
    }
    if (depth > 0) {
        fprintf(stderr, "parse: unmatched '('\n");
        exit(-1);
    }
    fprintf(stderr, "parse: could not find split\n");
    exit(-1);
}

void print_sexpr(struct node *sexpr) {
    char oper = '_';
    char twice = 0;
    switch (sexpr->type) {
    // atoms
    case 'v':
        printf("t");
        break;
    case 'n':
        printf("%d", sexpr->ival);
        break;
    // unary operators
    case 'u':
        if (sexpr->cval == '~') {
            printf("(~ ");
            print_sexpr(sexpr->rval);
            printf(")");
        } else {
            fprintf(stderr, "unexpected unary: %c", sexpr->cval);
        }
    // binary operators
    case 'b':
        if ((sexpr->cval == 'l') || (sexpr->cval == 'r')) {
            twice = 1;
            oper = '<';
            if (sexpr->cval == 'r')
                oper = '>';
        } else {
            oper = sexpr->cval;
        }
        if (strchr("+-*/^|&<>", oper) != NULL) {
            printf("(%c ", oper);
            if (twice)
                printf("%c", oper);
            print_sexpr(sexpr->lval);
            printf(" ");
            print_sexpr(sexpr->rval);
            printf(")");
        } else {
            fprintf(stderr, "unexpected binary: %c", sexpr->cval);
            exit(-1);
        }

    }
}

struct node *new_node(char type, char cval, unsigned int ival, 
                      struct node *lval, struct node *rval) {
    struct node *n;
    newest_node++;
    if (newest_node >= NUMNODE) {
        fprintf(stderr, "node table overrun\n");
        exit(-1);
    }
    n = &node_table[newest_node];
    n->type = type;
    n->cval = cval;
    n->ival = ival;
    n->lval = lval;
    n->rval = rval;
    return n;
}

