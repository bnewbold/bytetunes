#!/usr/bin/env sh

FORMULA="(t>>6)&(2*t)&(t>>1)"

if [ $1 ]; then
    FORMULA=$1
fi


echo "$FORMULA"
echo "main(t){for(t=0;;t++)putchar($FORMULA);}"  | gcc -x c -o /tmp/bytebeat - && /tmp/bytebeat | aplay -f U8 -r 8000 -c 1 -q
