/*
 * This crude interpolated sine lookup table implementation is useful for
 * testing PWM audio output from microcontrollers.
 *
 * If you want something that will actually sound like a sine wave, you should
 * bump up to 128 or more samples and higher bit resolution.
 */

#include <stdio.h>

/*
# python generator code for the below table
from math import sin
count = 64
print [int(127+127*sin(3.14159268*2*i/count)) for i in range(count)]
*/
unsigned char sine_lookup[] = {127, 139, 151, 163, 175, 186, 197, 207, 216,  
    225, 232, 239, 244, 248, 251, 253, 254, 253, 251, 248, 244, 239, 232, 225,
    216, 207, 197, 186, 175, 163, 151, 139, 126, 114, 102, 90, 78, 67, 56, 46,
    37, 28, 21, 14, 9, 5, 2, 0, 0, 0, 2, 5, 9, 14, 21, 28, 37, 46, 56, 67, 78,
    90, 102, 114}; 

unsigned char sin_8bit(int counter, int period);

unsigned char sin_8bit(int counter, int period) {
    int high, low;
    float t = (counter % period) / (float)period;
    float weight = (63*t) - (int)(63*t);
    low = sine_lookup[(int)(63*t)];
    if (63*t >= 62)
        //high = sine_lookup[0];
        high = 118;    // not sure why sine_lookup[0] creates a glitch...
    else
        high = sine_lookup[1+(int)(63*t)];
    //printf("\tl=%d h=%d w=%f\n", low, high, weight);    
    return (int)(high * weight + low * (1.0 - weight));
}   

void main() {
    int i;
    for(i=0; i<800;i++){
        printf("%d\t%d\n", i, sin_8bit(i, 800));
    }
}

