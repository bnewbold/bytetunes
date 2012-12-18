#!/usr/bin/env python

from expr import *

import sys

def play(s):
    machine = parse(preparse(tokenize(s)))
    t = 0
    while True:
        t += 1
        sys.stdout.write(chr(execute(machine, t) & 0x000000FF))

def main():
    if len(sys.argv) <= 1:
        play("(t>>6)&(2*t)&(t>>1)")
    elif sys.argv[1] == '--test':
        test_tokenize()
        test_preparse()
        test_parse()
    else:
        play(sys.argv[1])

if __name__=='__main__':
    main()
