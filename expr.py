
"""
PARENS:     '(' (EXPR | ATOM) ')'
EXPR:       (BINARY | PARENS | NOT)
BINARY:     (ATOM | EXPR) BOPER (ATOM | EXPR)
UNITARY:    UOPER (EXPR | ATOM)
ATOM:       (VAR | NUM)
VAR:        't'
NUM:        <integer>


Order of ops:
    ()
    ~
    * / %
    + -
    << >>
    &
    ^
    |

TODO: allow negative numbers?
"""

hexchr = "0123456789ABCDEFabcdef"
raw_opers = "+ - * / % << >> & | ^ ~".split()
binary_opers = list("+-*/%lr&|^")
unitary_opers = list("~+-")

binaryoper_priority = (
    tuple("|"),
    tuple("^"),
    tuple("&"),
    tuple("rl"),
    tuple("+-"),
    tuple("*/%"),
)

def cons(a, l):
    return [a, ] + l

def car(l):
    return l[0]

def cdr(l):
    return l[1:]

def preparse(l, depth=0):
    """
    match parens and set types
    """
    ret = []
    for i in range(len(l)):
        if l[i] == '(':
            #print "%sleft: %s" % (" "*depth, ret)
            middle, remains = preparse(l[i+1:], depth+1)
            #print "%smiddle: %s" % (" "*depth, middle)
            right, moreright = preparse(remains, depth)
            #print "%sright: %s" % (" "*depth, right)
            return ret + [tuple(cons('PARENS', middle))] + right, moreright
        elif l[i] == ')':
            if depth == 0:
                Exception("preparse: unexpected parens")
            return ret, l[i+1:]
        else:
            if type(l[i]) == int:
                ret.append(('NUM', l[i]))
            elif l[i] == '~':
                ret.append(('NOTOPER',))
            elif l[i] in binary_opers:
                ret.append(('OPER', l[i]))
            elif l[i] == 't':
                ret.append(('VAR',))
            else:
                raise Exception("prepase: unexpected token: %s" % l[i])
    if depth > 0:
        raise Exception("preparse: unmatched parens")
    return ret, []

def parse(l):
    #print l
    if not l or len(l) == 0:
        return []
    if len(l) == 1:
        if car(car(l)) in ('VAR', 'NUM'):
            return ('ATOM', car(l))
        elif car(car(l)) == 'PARENS':
            return parse(cdr(car(l)))
        else:
            raise Exception("parse error: %s" % l)
    # ~ has highest priority
    if ('NOTOPER',) in l:
        i = l.index(('NOTOPER',))
        if i == len(l)-1:
            raise Exception("parse error: trailing ~")
        return parse(l[:i] + ('NOT', parse(l[i+1])) + l[i+2:])
    # helper function
    def hunt_opers(l, operlist):
        for i in range(1, len(l) - 1):
            if car(l[i]) == 'OPER' and l[i][1] in operlist:
                left = parse(l[:i])
                right = parse(l[i+1:])
                if not car(left) in ['ATOM', 'BINARY', 'NOT']:
                    raise Exception("parse error: not a binary operand: %s" % 
                                    left)
                if not car(right) in ['ATOM', 'BINARY', 'NOT']:
                    raise Exception("parse error: not a binary operand: %s" %
                                    right)
                return ('BINARY', l[i][1], left, right)
    for operlist in binaryoper_priority:
        match = hunt_opers(l, operlist)
        if match:
            return match
    raise Exception("parse error: %s" % [l])

def schemify(ast):
    if car(ast) == 'BINARY':
        return "(%s %s %s)" % (ast[1], schemify(ast[2]), schemify(ast[3]))
    elif car(ast) == 'NOT':
        return "(~ %s)" % schemify(ast[1])
    elif car(ast) == 'ATOM':
        return "%s" % ast[1][1]
    else:
        raise Exception("schemify: bad type: %s" % ast)

def test_parse():
    """
    ()
    ~~1
    """

def tokenize(s):
    s = s.strip()
    if not s or len(s) is 0:
        return []
    if car(s) in ['t', 'i']:
        # accept i, but tokenize as t
        return cons('t', tokenize(cdr(s)))
    if car(s) in raw_opers or car(s) in ['(', ')']:
        return cons(car(s), tokenize(cdr(s)))
    if len(s) >= 2 and s[:2] == '>>':
        return cons('r', tokenize(s[2:]))
    if len(s) >= 2 and s[:2] == '<<':
        return cons('l', tokenize(s[2:]))
    if len(s) >= 3 and s[:2] == "0x" and s[2] in hexchr:
        num = s[:3]
        for c in s[3:]:
            if c in hexchr:
                num += c
            else:
                break
        return cons(int(num, 16), tokenize(s[len(num):]))
    if car(s).isdigit():
        num = car(s)
        for c in s[1:]:
            if c.isdigit():
                num += c
            else:
                break
        return cons(int(num), tokenize(s[len(num):]))
    # default
    raise Exception("tokenization error: %s" % s)

def execute(ast, t):
    if car(ast) == 'ATOM':
        if car(ast[1]) == 'VAR':
            return t
        if car(ast[1]) == 'NUM':
            return ast[1][1]
    elif car(ast) == 'NOT':
        return ~ execute(ast[1], t)
    elif car(ast) == 'BINARY':
        # UGH
        oper, left, right = ast[1:4]
        left = execute(ast[2], t)
        right = execute(ast[3], t)
        if oper == '+':
            return left + right
        elif oper == '-':
            return left - right
        elif oper == '*':
            return left * right
        elif oper == '/':
            return left / right
        elif oper == '%':
            return left % right
        elif oper == 'r':
            return left >> right
        elif oper == 'l':
            return left << right
        elif oper == '|':
            return left | right
        elif oper == '^':
            return left ^ right
        elif oper == '&':
            return left & right
    raise Exception("execute: unexpected node: %s" % car(ast))


# ==============================
def test_tokenize():
    assert tokenize("(t>>4)") == ['(','t','r',4,')']
    assert tokenize("t") == ['t']
    assert tokenize("i") == ['t']
    assert tokenize("tt123i") == ['t','t',123,'t']
    assert tokenize("12345") == [12345]
    assert tokenize("0xF") == [15]
    assert tokenize("<<") == ['l']
    assert tokenize(">>") == ['r']
    assert tokenize("~") == ['~']
    assert tokenize("444 444") == [444,444]
    assert tokenize("t*(((t>>12)|(t>>8))&(63&(t>>4)))") == \
        ['t','*','(','(','(','t','r',12,')','|','(','t','r',8,')',')',
         '&','(',63,'&','(','t','r',4,')',')',')']
    try:
        assert tokenize("1.0") is "TOKENIZE ERROR"
    except:
        pass
    try:
        assert tokenize("t,(") is "TOKENIZE ERROR"
    except:
        pass
    try:
        assert tokenize("0x") is "TOKENIZE ERROR"
    except:
        pass
    try:
        assert tokenize("1230x123") is "TOKENIZE ERROR"
    except:
        pass
    try:
        assert tokenize("x123") is "TOKENIZE ERROR"
    except:
        pass

