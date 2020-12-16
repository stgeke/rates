#include <math.h>

#define __device__
#include "gri.inc"

extern "C" void ckwxp_(double * P, double * T, double * x, int * iwrk, double *rwrk, double * wdot);

/*Returns the molar production rate of species */
/*Given P, T, and mole fractions */
void ckwxp_(double * P, double * T, double * x, int * iwrk, double * rwrk, double * wdot)
{
    int id; /*loop counter */
    double c[53]; /*temporary storage */
    double PORT = 1.0e6 * (*P)/(8.314621000e+07 * (*T)); /*1.0e6 * P/RT so c goes to SI units */
    /*Compute conversion, see Eq 10 */
    for (id = 0; id < 53; ++id) {
        c[id] = x[id]*PORT;
    }

    /*convert to chemkin units */
    productionRate(wdot, c, *T);

    /*convert to chemkin units */
    for (id = 0; id < 53; ++id) {
        wdot[id] *= 1.0e-6;
    }
}


