#!/usr/bin/env python

from expr import *

import sys

DEFAULT = "(t>>6)&(2*t)&(t>>1)"

def play(s):
    machine = parse(preparse(tokenize(s)))
    t = 0
    while True:
        t += 1
        sys.stdout.write(chr(execute(machine, t) & 0x000000FF))

def main():
    if len(sys.argv) <= 1:
        play(DEFAULT)
    elif sys.argv[1] == '--test':
        test_tokenize()
        test_preparse()
        test_parse()
    elif sys.argv[1] == '--parse':
        if len(sys.argv) < 3:
            print strmachine(parse(preparse(tokenize(DEFAULT))))
        else:
            print strmachine(parse(preparse(tokenize(sys.argv[2]))))
    else:
        play(sys.argv[1])

if __name__=='__main__':
    main()
