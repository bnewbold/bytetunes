
#include "wirish.h"
//#include <stdio.h>
#include <string.h>
//#include <stdlib.h>

#define DEFAULT "(& (>> t 6) (& (* 2 t) (>> t 1)))"
#define NUMNODE 160
#define PWM_OUT_PIN     33

struct node {
    char type;
    char cval;
    unsigned int ival;
    struct node *lval;
    struct node *rval;
};

int isdigit(char);
void print_sexpr(struct node *sexpr);
int execute(struct node *sexpr, unsigned int t);
int find_split(char* s, int start, int end);
struct node *parse(char *s, int start, int end);
struct node *new_node(char type, char cval, unsigned int ival,
                      struct node *lval, struct node *rval);

static struct node active_table[NUMNODE];
static struct node node_table[NUMNODE];
static int newest_node = 0;
node *machine;
node *new_machine;
void handler_sample(void);

unsigned char sin_8bit(int counter, int period);
char inbuffer[256];

int sstrlen(char *s, int max);

HardwareTimer gen(1);
HardwareTimer pwm(4);

int counter = 0;

int isdigit(char c) {
    return (c >= '0' and c <= '9');
}

/*
from math import sin
count = 64
print [int(127+127*sin(3.14159268*2*i/count)) for i in range(count)]
*/
unsigned char sine_lookup[] __FLASH__ = {127, 139, 151, 163, 175, 186, 197, 207, 216,
    225, 232, 239, 244, 248, 251, 253, 254, 253, 251, 248, 244, 239, 232, 225,
    216, 207, 197, 186, 175, 163, 151, 139, 126, 114, 102, 90, 78, 67, 56, 46,
    37, 28, 21, 14, 9, 5, 2, 0, 0, 0, 2, 5, 9, 14, 21, 28, 37, 46, 56, 67, 78,
    90, 102, 114};

void setup() {
    int i;
    pinMode(PWM_OUT_PIN, OUTPUT);
    pinMode(31, OUTPUT);
    pinMode(33, OUTPUT);
    pinMode(16, PWM);
    pinMode(4, OUTPUT);
    digitalWrite(1, 1);

    // initialize with DEFAULT machines
    machine = parse((char*)DEFAULT, 0, strlen((char*)DEFAULT)-1);
    for (i=0;i<NUMNODE;i++) {
        active_table[i] = node_table[i];
    }

    // configure PWM output
    pinMode(PWM_OUT_PIN, OUTPUT);
    pwm.setMode(1, TIMER_PWM);
    pwm.setPrescaleFactor(1);
    pwm.setOverflow(255);       // 8-bit resolution
    pwm.setCompare(3, 128);        // initialize to "zero"


    // configure 8KHz ticker and interrupt handler
    gen.pause();
    gen.setPeriod(125);         // 8Khz
    gen.setCompare(1, 1);
    gen.setMode(1, TIMER_OUTPUT_COMPARE);
    gen.attachInterrupt(1, handler_sample);
    gen.refresh();

    // get things started!
    gen.resume();
}

int inbuffer_index;

void loop() {
    int len, i;
    SerialUSB.println();
    SerialUSB.println("Currently playing:");
    print_sexpr(machine);
    SerialUSB.println();
    SerialUSB.println("Input a tune in exact s-expr syntax:");
    SerialUSB.print("> ");
    inbuffer_index = 0;
    while (1) {
        inbuffer[inbuffer_index] = SerialUSB.read();
        SerialUSB.print(inbuffer[inbuffer_index]);
        if (inbuffer[inbuffer_index] == 8) {
            if (inbuffer_index > 0) {
                inbuffer_index--;
            }
        } else if (inbuffer[inbuffer_index] == '\n' ||
                   inbuffer[inbuffer_index] == '\r') {
            inbuffer[inbuffer_index] = '\0';
            SerialUSB.println();
            break;
        } else {
            inbuffer_index++;
        }
        if (inbuffer_index == 256) {
            SerialUSB.println("\n\rInput too long!");
            return;
        }
    }
    len = sstrlen(inbuffer, 256);
    if (len == 256 || len < 1) {
        SerialUSB.println("Invalid input!");
        return;
    }
    newest_node = 0;
    new_machine = parse(inbuffer, 0, len-2);
    if (new_machine == NULL) {
        return;
    }
    // swap in new machine
    gen.pause();
    for (i=0;i<NUMNODE;i++) {
        active_table[i] = node_table[i];
    }
    machine = new_machine;
    SerialUSB.println();
    SerialUSB.println("Playing new tune:");
    print_sexpr(machine);
    SerialUSB.println();
    SerialUSB.println();
    gen.resume();
}

void handler_sample(void) {
    digitalWrite(33, 1);
    digitalWrite(31, 1);
    //pwm.setCompare(1, sin_8bit(counter, 799));  // 10Hz sine wave
    pwm.setCompare(1, (0x000000FF & execute(machine, counter)));
    //pwmWrite(16, sin_8bit(counter, 800));
    counter++;
    //SerialUSB.print('.');
    digitalWrite(33, 0);
    digitalWrite(31, 0);
}

unsigned char sin_8bit(int counter, int period) {
    int high, low;
    float t = (counter % period) / (float)period;
    float weight = t - (int)t;
    low = sine_lookup[(int)(63*t)];
    if (63*t > 62)
        //high = sine_lookup[0];
        high = 118;
    else
        high = sine_lookup[1+(int)(63*t)];

    return (int)(high * weight + low * (1.0 - weight));
}

int sstrlen(char *s, int max) {
    int i;
    for (i=0; i<max; i++) {
        if (s[i] == '\n' || s[i] == '\0') {
            return i+1;
        }
    }
    return max;
}

int digtoi(char *s, int start, int end) {
    int num = 0;
    for(;start<=end;start++) {
        if (! isdigit(s[start])) {
            SerialUSB.print("parse: not a digit: ");
            SerialUSB.println(s[start]);
            return -1;
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
        SerialUSB.print("parse: unparsable: ");
        for (;start<=end;start++) {
            SerialUSB.print(s[start]);
        }
        SerialUSB.print("\n");
        return NULL;
    }

    // unitary operator
    if (s[start+1] == '~') {
        rval = parse(s, start+3, end-1);
        return new_node('u', '~', 0, NULL, rval);
    }

    // various binary operators
    if ( strchr("*/%+-&^|<>", s[start+1]) != NULL) {
        cval = s[start+1];
        offset = 3;
        if (cval == '<' && s[start+2] == '<' && s[start+3] == ' ') {
            cval = 'l';
            offset = 4;
        } else if (cval == '>' && s[start+2] == '>' && s[start+3] == ' ') {
            cval = 'r';
            offset = 4;
        } else if (s[start+2] != ' ') {
            SerialUSB.print("parse: invalid operator: ");
            SerialUSB.println(cval);
            return NULL;
        }
        split = find_split(s, start+offset, end-1);
        lval = parse(s, start+offset, split-1);
        rval = parse(s, split+1, end-1);
        return new_node('b', cval, 0, lval, rval);
    }

    // should not get here 
    SerialUSB.print("parse: feel through\n");
    return NULL;
}

int execute(struct node *sexpr, unsigned int t) {
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
            SerialUSB.print("unexpected unary oper: ");
            SerialUSB.println(sexpr->cval);
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
            SerialUSB.print("unexpected binary oper: ");
            SerialUSB.print(sexpr->cval);
            return NULL;
            // XXX: halt
        }
    default:
        SerialUSB.print("execute: unknown type: ");
        SerialUSB.print(sexpr->type);
        return NULL;
        // XXX: halt
    }
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
            SerialUSB.print("parse: unmatched ')'\n");
            return -1;
            // XXX: fail
        }
    }
    if (depth > 0) {
        SerialUSB.print("parse: unmatched '('\n");
        return -1;
        // XXX: fail
    }
    SerialUSB.print("parse: could not find split\n");
    return -1;
    // XXX: fail
}

void print_sexpr(struct node *sexpr) {
    char oper = '_';
    char twice = 0;
    switch (sexpr->type) {
    // atoms
    case 'v':
        SerialUSB.print('t');
        break;
    case 'n':
        SerialUSB.print(sexpr->ival);
        break;
    // unary operators
    case 'u':
        if (sexpr->cval == '~') {
            SerialUSB.print("(~ ");
            print_sexpr(sexpr->rval);
            SerialUSB.print(")");
        } else {
            SerialUSB.print("unexpected unary: ");
            SerialUSB.println(sexpr->cval);
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
        if (strchr("+-*/^|&<>%", oper) != NULL) {
            SerialUSB.print('(');
            SerialUSB.print(oper);
            SerialUSB.print(' ');
            if (twice)
                SerialUSB.print(oper);
            print_sexpr(sexpr->lval);
            SerialUSB.print(' ');
            print_sexpr(sexpr->rval);
            SerialUSB.print(')');
        } else {
            SerialUSB.print("unexpected binary: ");
            SerialUSB.println(sexpr->cval);
            // XXX:
        }

    }
}

struct node *new_node(char type, char cval, unsigned int ival, 
                      struct node *lval, struct node *rval) {
    struct node *n;
    newest_node++;
    if (newest_node >= NUMNODE) {
        SerialUSB.print("node table overrun\n");
        // XXX:
    }
    n = &node_table[newest_node];
    n->type = type;
    n->cval = cval;
    n->ival = ival;
    n->lval = lval;
    n->rval = rval;
    return n;
}

// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need libmaple may fail.
__attribute__((constructor)) void premain() {
    init();
}

int main(void) {
    setup();

    while (true) {
        loop();
    }

    return 0;
}
