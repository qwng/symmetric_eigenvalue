#include "eigenvalues.h"

#include <omp.h>
#include <math.h>
#include <assert.h>
#include "mkl.h"

#include "helper.h"

inline double secularEquation(double lambda, double roh, double* z, double* D, int n) {
    double sum = 0;
    int i;
    #pragma omp parallel for default(shared) private(i) schedule(static) reduction(+:sum)
    for (i = 0; i < n; ++i)
        sum += z[i]*z[i] / (D[i]-lambda);
    return 1+roh*sum;
}

double* computeEigenvalues(double* D, double* z, int n, double beta, double theta) {
    /*
     * Store eigenvalues in new array (do not overwrite D), since the elements in D are needed later on to compute the eigenvectors)S
     */
    double* L = malloc(n * sizeof(double));

    double roh = beta * theta;
    assert(roh != 0);

    /* Note, if roh > 0, then the last eigenvalue is behind the last d_i
     * If roh < 0, then the first eigenvalue is before the first d_i */

    // use norm of z as an approximation to find the first resp. last eigenvalue
    double normZ = cblas_dnrm2(n, z, 1);

    /******************
     * Simple Bisection algorithm
     * ****************/
    long maxIter = 10000;
    double eps = 1e-10;
    /*
    N ← 1
    While N ≤ NMAX # limit iterations to prevent infinite loop
      c ← (a + b)/2 # new midpoint
      If f(c) = 0 or (b – a)/2 < TOL then # solution found
        Output(c)
        Stop
      EndIf
      N ← N + 1 # increment step counter
      If sign(f(c)) = sign(f(a)) then a ← c else b ← c # new interval
    EndWhile
    */
    int i;
    //#pragma omp parallel for default(shared) private(i) schedule(static)
    for (i = 0; i < n; ++i) { // for each eigenvalue
        double lambda = 0;
        double a, b; // interval boundaries
        // set initial interval
        if (roh < 0) {
            if (i == 0) {
                a = D[i] - normZ;
                int j = 0;
                while(secularEquation(a, roh, z, D, n) < 0) {
                    a -= normZ;
                    assert(++j < 100);
                }
            } else {
                a = D[i-1];
            }
            b = D[i];
        } else {
            a = D[i];
            if (i == n-1) {
                b = D[i] + normZ;
                int j = 0;
                while(secularEquation(b, roh, z, D, n) < 0) {
                    b += normZ;
                    assert(++j < 100);
                }
            } else {
                b = D[i+1];
            }
        }

        int j = 0;
        while (++j < maxIter) {
            lambda = (a+b) / 2;
            if (secularEquation(lambda, roh, z, D, n) == 0 || (b-a)/2 < eps)
                break;

            // if sign(a) == sign(lambda)
            if ((a >= 0 && lambda >= 0) || (a < 0 && lambda < 0))
                a = lambda;
            else
                b = lambda;
        }
        L[i] = lambda;
    }

    return L;
}

double* computeNormalizationFactors(double* D, double* z, double* L, int n) {
    double *N = malloc(n * sizeof(double));

    int i, j;
    double tmp;
    #pragma omp parallel for default(shared) private(i,j,tmp) schedule(static)
    for (i = 0; i < n; ++i) {
        N[i] = 0;
        for (j = 0; j < n; ++j) {
            tmp = D[j]-L[i];
            N[i] += z[j]*z[j] / (tmp*tmp);
        }

        N[i] = sqrt(N[i]);
    }

    return N;
}
