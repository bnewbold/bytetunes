/*
 * bytebeat.cpp - parse and play bytebeat songs
 * Date: December 2012
 * Author: bnewbold@robocracy.org
 *
 * For use with libmaple, to run on ARM Cortex-M3 microcontrollers.
 *
 * This version only parses tunes in "perfect" S-EXPR format.
 *
 * Outputs PWM audio on pin 16 and listens on SerialUSB for new tunes.
 *
 * See TODO file for problems.
 *
 * This file is released under the General Public License version 3 (GPLv3).
 */

#include "wirish.h"
#include <string.h>

#define DEFAULT_TUNE    "(& (>> t 6) (& (* 2 t) (>> t 1)))"
#define NUMNODE         160
#define LED_PIN         33
#define MONITOR_PIN     31

// S-EXPR data structure stuff
struct node {
    char type;
    char cval;
    unsigned int ival;
    struct node *lval;
    struct node *rval;
};
struct node *new_node(char type, char cval, unsigned int ival,
                      struct node *lval, struct node *rval);
static struct node active_table[NUMNODE];
static struct node node_table[NUMNODE];
static int newest_node = 0;

// S-EXPR parser stuff
int isdigit(char);
void print_sexpr(struct node *sexpr);
int find_split(char* s, int start, int end);
struct node *parse(char *s, int start, int end);
int digtoi(char *s, int start, int end);

// bytetune machine output stuff
int execute(struct node *sexpr, unsigned int t);
node *machine;
node *new_machine;
void handler_sample(void);
HardwareTimer gen(1);
HardwareTimer pwm(4);
int counter = 0;

// other stuff
char inbuffer[256];
int inbuffer_index;
int sstrlen(char *s, int max);

// ====================== primary control flow functions ======================

// runs once at power up to configure hardware peripherals
void setup() {
    int i;

    // for monitoring interrupt loop length
    pinMode(LED_PIN, OUTPUT);
    pinMode(MONITOR_PIN, OUTPUT);

    // initialize with DEFAULT_TUNE
    machine = parse((char*)DEFAULT_TUNE, 0, strlen((char*)DEFAULT_TUNE)-1);
    for (i=0;i<NUMNODE;i++) {
        active_table[i] = node_table[i];
    }

    // configure PWM output on pin 16 (Timer 4, Channel 1)
    pinMode(16, PWM);               // primary PWM audio output
    pwm.setMode(1, TIMER_PWM);
    pwm.setPrescaleFactor(1);
    pwm.setOverflow(255);           // 8-bit resolution
    pwm.setCompare(3, 128);         // initialize to "zero"

    // configure 8KHz ticker and interrupt handler
    gen.pause();
    gen.setPeriod(125);             // 8Khz
    gen.setCompare(1, 1);
    gen.setMode(1, TIMER_OUTPUT_COMPARE);
    gen.attachInterrupt(1, handler_sample);
    gen.refresh();

    // get things started!
    gen.resume();
}

// runs continuously, reading new machines from SerialUSB input
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
        // read in characters one at a time
        inbuffer[inbuffer_index] = SerialUSB.read();
        if (inbuffer[inbuffer_index] > 127) {
            // if not an ASCII character ignore it
            continue;
        }
        SerialUSB.print(inbuffer[inbuffer_index]);
        if (inbuffer[inbuffer_index] == '\n' ||
                   inbuffer[inbuffer_index] == '\r') {
            // on submit, zero terminate the string and break out to parse
            inbuffer[inbuffer_index] = '\0';
            SerialUSB.println();
            break;
        }
        inbuffer_index++;
        if (inbuffer_index == 256) {
            SerialUSB.println("\n\rInput too long!");
            return;
        }
    }
    len = sstrlen(inbuffer, 256);
    if (len == 256 || len < 1) {
        // too long or short
        SerialUSB.println("Invalid input!");
        return;
    }
    // ok, we're going to try parsing
    newest_node = 0;
    new_machine = parse(inbuffer, 0, len-2);
    if (new_machine == NULL) {
        return;
    }
    // if we got this far, swap in the new machine
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

// gets called by 8KHz timer interrupt. pushes out the next audio sample to PWM
// output
void handler_sample(void) {
    // set LED and monitor line high (so we can check with a 'scope how long
    // this function call lasts)
    digitalWrite(LED_PIN, 1);
    digitalWrite(MONITOR_PIN, 1);

    // execute and write truncated result
    pwm.setCompare(1, (0x000000FF & execute(machine, counter)));
    counter++;

    // set LED and monitor line low
    digitalWrite(LED_PIN, 0);
    digitalWrite(MONITOR_PIN, 0);
}

int main(void) {
    setup();
    while (true) { loop(); }
    return 0;
}

// ====================== tune parsing and playback functions =================

// does what it says; returns NULL if there is a problem
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

// calculates a single audio sample (index 't') from tune 'sexpr'.
// returns an int, which should be truncated to 8bit length for playback
// if there is a problem, tries to print out to SerialUSB and returns "zero"
// value
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
            return 127;
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
            return 127;
        }
    default:
        SerialUSB.print("execute: unknown type: ");
        SerialUSB.print(sexpr->type);
        return 127;
    }
}

// finds the seperating whitespace character between the two arguments to a
// binary S-EXPR operator
// returns either the index of the seperator or -1 if there was a problem
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
        }
    }
    if (depth > 0) {
        SerialUSB.print("parse: unmatched '('\n");
        return -1;
    }
    SerialUSB.print("parse: could not find split\n");
    return -1;
}

// prints out the S-EXPR to SerialUSB
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
            SerialUSB.println();
            SerialUSB.print("print_sexpr: unexpected unary: ");
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
            SerialUSB.println();
            SerialUSB.print("print_sexpr: unexpected binary: ");
            SerialUSB.println(sexpr->cval);
            return;
        }

    }
}

// creates a new node struct in the node table
struct node *new_node(char type, char cval, unsigned int ival, 
                      struct node *lval, struct node *rval) {
    struct node *n;
    newest_node++;
    if (newest_node >= NUMNODE) {
        SerialUSB.println();
        SerialUSB.println("node table overrun!");
        return NULL;
    }
    n = &node_table[newest_node];
    n->type = type;
    n->cval = cval;
    n->ival = ival;
    n->lval = lval;
    n->rval = rval;
    return n;
}

// ====================== misc helper functions ======================

// "safe" string length. breaks on newline or NULL char, checks at most 'max'
// characters
int sstrlen(char *s, int max) {
    int i;
    for (i=0; i<max; i++) {
        if (s[i] == '\n' || s[i] == '\0') {
            return i+1;
        }
    }
    return max;
}

// finds a (positive) integer.
// returns -1 if there is a problem.
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

int isdigit(char c) {
    return (c >= '0' and c <= '9');
}

// libmaple-specific re-definition
// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need libmaple may fail.
__attribute__((constructor)) void premain() {
    init();
}

