
from expr import *

import sys

def play(s):
    machine = parse(preparse(tokenize(s))[0])
    t = 0
    while True:
        t += 1
        sys.stdout.write(chr(execute(machine, t) & 0x000000FF))

def main():
    play("(t>>6)&(2*t)&(t>>1)")

if __name__=='__main__':
    main()
