#include <stdio.h>
#include <math.h>
#include <complex.h>
#include "header.h"

#define AXIS_DIV 46.0
#define REAL_AXIS_STEP ((REAL_HI - REAL_LO) / AXIS_DIV)
#define IMAG_AXIS_STEP ((IMAG_HI - IMAG_LO) / AXIS_DIV)

#define IN_CHAR '*'
#define OUT_CHAR ' '

int mandelbrot_in_set(double complex c)
{
    int i;
    double complex z = 0;
    for (i = 0; i < MAX_ITERATION; i++)
    {
        z = cpow(z, 2) + c;
        if (cabs(z) > _INFINITY)
            return i;
    }
    return -1;
}

void mandelbrot_print(void)
{
    for (double i = IMAG_LO; i < IMAG_HI; i += IMAG_AXIS_STEP)
    {
        for (double r = REAL_LO; r < REAL_HI; r += REAL_AXIS_STEP)
        {
            if (mandelbrot_in_set(r + i * I))
                putchar(IN_CHAR);
            else
                putchar(OUT_CHAR);
            putchar(' ');
        }
        putchar('\n');
    }
}

/*
#define SQUARE(x) (pow((x), 2))
#define SQUARE_CML(z) (SQUARE(creal(z)) - SQUARE(cimag(z)) + 2 * creal(z) * cimag(z) * I)

double magnitude(double complex z) { return sqrt(SQUARE(creal(z)) + SQUARE(cimag(z))); }
*/
