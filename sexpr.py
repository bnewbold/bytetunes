#!/usr/bin/env python
"""
This code is intentionally very c-style
"""

import sys

node_table = []
DEFAULT = "(& (>> t 6) (& (* 2 t) (>> t 1)))"

# node = dict(type="", cval="", ival=0, lval=0, rval=0)

def find_split(s, start, end):
    """
    this function searches for the split between arguments in a binary sexpr
    """
    depth = 0
    for i in range(start, end):
        if depth == 0 and s[i] == ' ':
            return i
        if s[i] == '(':
            depth += 1
        if s[i] == ')':
            depth -= 1
        if depth < 0:
            raise Exception("parse: unmatched ')'")
    if depth > 0:
        raise Exception("parse: unmatched '('")
    return -1

def parse(s, start, end):
    """
    we are going to get either an sexpr (wrapped in parens) or an atom
    """
    # number
    if s[start] in "0123456789":
        return dict(type='n', ival=int(s[start:end+1]))

    # variable
    if s[start] == 't' and end-start == 0:
        return dict(type='v')

    # must be an s-exp
    if not (s[start] == '(' and s[end] == ')') or end-start < 4:
        raise Exception("parse: unparsable: '%s'" % str(s[start:end+1]))

    # unitary operator
    if s[start+1] == '~':
        rval = parse(s, start+3, end-1)
        return dict(type='u', cval='~', rval=rval)

    # various binary operators
    if s[start+1] in "*/%+-&^|<>":
        cval = s[start+1]
        offset = 3
        if cval == '<' and s[start+2] == '<' and s[start+3] == ' ':
            cval = 'l'
            offset = 4
        elif cval == '>' and s[start+2] == '>' and s[start+3] == ' ':
            cval = 'r'
            offset = 4
        elif s[start+2] != ' ':
            raise Exception("parse: invalid operator?")
        split = find_split(s, start+offset, end-1)
        lval = parse(s, start+offset, split-1)
        rval = parse(s, split+1, end-1)
        return dict(type='b', cval=cval, lval=lval, rval=rval)

    # should not get here 
    raise Exception("parse: feel through")

def ezparse(s):
    s = s.strip()
    return parse(s, 0, len(s)-1)

def strmachine(sexpr):
    #print sexpr
    # atom
    if sexpr['type'] == 'v':
        return 't'
    elif sexpr['type'] == 'n':
        return str(sexpr['ival'])
    # unary
    elif sexpr['type'] == 'u':
        if sexpr['cval'] == '~':
            return "(~ %s)" % strmachine(sexpr['rval'])
        raise Exception("unexpected unary: %s" % sexpr['cval'])
    # binary
    elif sexpr['type'] == 'b':
        if sexpr['cval'] == 'l' or sexpr['cval'] == 'r':
            oper = '<'
            if sexpr['cval'] == 'r':
                oper = '>'
            return "(%s%s %s %s)" % (oper, oper, strmachine(sexpr['lval']), strmachine(sexpr['rval']))
        elif sexpr['cval'] in '+-*/^|&':
            return "(%s %s %s)" % (sexpr['cval'], strmachine(sexpr['lval']), strmachine(sexpr['rval']))
        raise Exception("unexpected binary: %s" % sexpr['cval'])

def execute(sexpr, t):
    # atom
    if sexpr['type'] == 'v':
        return t
    elif sexpr['type'] == 'n':
        return sexpr['ival']
    # unary
    elif sexpr['type'] == 'u':
        if sexpr['cval'] == '~':
            return ~ execute(sexpr['rval'], t)
        raise Exception("unexpected unary: %s" % sexpr['cval'])
    # binary
    elif sexpr['type'] == 'b':
        if sexpr['cval'] == '+':
            return execute(sexpr['lval'], t) + execute(sexpr['rval'], t)
        if sexpr['cval'] == '-':
            return execute(sexpr['lval'], t) - execute(sexpr['rval'], t)
        if sexpr['cval'] == '*':
            return execute(sexpr['lval'], t) * execute(sexpr['rval'], t)
        if sexpr['cval'] == '/':
            return execute(sexpr['lval'], t) / execute(sexpr['rval'], t)
        if sexpr['cval'] == '^':
            return execute(sexpr['lval'], t) ^ execute(sexpr['rval'], t)
        if sexpr['cval'] == '&':
            return execute(sexpr['lval'], t) & execute(sexpr['rval'], t)
        if sexpr['cval'] == '|':
            return execute(sexpr['lval'], t) | execute(sexpr['rval'], t)
        if sexpr['cval'] == 'r':
            return execute(sexpr['lval'], t) >> execute(sexpr['rval'], t)
        if sexpr['cval'] == 'l':
            return execute(sexpr['lval'], t) << execute(sexpr['rval'], t)
        raise Exception("unexpected binary: %s" % sexpr['cval'])
    pass

def play(machine):
    t = 0
    while True:
        t += 1
        sys.stdout.write(chr(execute(machine, t) & 0x000000FF))

def test_parse():
    tlist = [
        '1',
        't',
        '(+ 1 t)',
        '(~ (~ 1))',
        '(+ (^ t 123) (| 300 t))',
        DEFAULT,
    ]
    for t in tlist:
        assert strmachine(ezparse(t)) == t, "\n%s <- GOT\n%s <- EXPECTED" % (strmachine(ezparse(t)), t)
    flist = [
        '',
        '~',
        '~~1',
        '(+ + +)',
        '(+ 1 1))',
        '(+ 1 1',
    ]
    for f in flist:
        try:
            assert strmachine(ezparse(t)) == "SHOULDFAIL", "%s <- SHOULD HAVE FAILED"% t
        except:
            pass
    print "passed all parse tests!"

def test_execute():
    assert execute(ezparse('t'), 12345) == 12345
    assert execute(ezparse('(+ t 6)'), 5) == 11
    assert execute(ezparse('(& t 99)'), 0x12345) == 0x12345 & 99
    print "passed all execute tests!"

def main():
    if len(sys.argv) <= 1:
        play(ezparse(DEFAULT))
    elif sys.argv[1] == '--test':
        test_parse()
        test_execute()
    elif sys.argv[1] == '--parse':
        if len(sys.argv) < 3:
            print strmachine(ezparse(DEFAULT))
        else:
            print strmachine(ezparse(sys.argv[2]))
    else:
        play(sys.argv[1])

if __name__=='__main__':
    main()
