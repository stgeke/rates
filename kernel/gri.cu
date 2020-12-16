/*Returns the molar production rate of species */
/*Given P, T, and mole fractions */
extern "C" __global__ void ckwxp_(const int N, double * P, double * T, double * x, double * wdot)
{
    double c[53]; /*temporary storage */
    double rates[53] = {0.0}; /*temporary storage */
    const int idx = blockDim.x * blockIdx.x + threadIdx.x;

    if (idx >= N)  return;

    const double T_val = T[idx];
    const double PORT = 1.0e6 * P[idx]/(8.314621000e+07 * T_val); /*1.0e6 * P/RT so c goes to SI units */

#pragma unroll
    for (int id = 0; id < 53; ++id) {
        c[id] = x[id*N + idx]*PORT;
    }

    /*convert to chemkin units */
    productionRate(rates, c, T_val);

#pragma unroll
    for (int id = 0; id < 53; ++id) {
        wdot[id*N + idx] = 1.0e-6 * rates[id];
    }
}


