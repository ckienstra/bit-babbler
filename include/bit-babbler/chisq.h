//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2015,  Ron <ron@debian.org>
//
// The implementation of poz() and pochisq() were taken almost verbatim from
// the public domain ENT test suite, which in turn also took it with minimal
// modification from public domain code by Gary Perlman of the Wang Institute.
// The fine tradition of minimal modification is continued here, with changes
// mostly to keep modern gcc from barking at us about it and to encapsulate it
// a bit more tightly.  So as is usual in such cases, the blame for any damage
// to Gary and John's careful work entirely stops here.
//
// But if it works perfectly, all credit rightly belongs to them.
//
// The original code for ENT can be found at <http://www.fourmilab.ch/random>
// The changes to poz() and pochisq() made in this file are released into the
// public domain as well.

#ifndef _BB_CHISQ_H
#define _BB_CHISQ_H

#include <cmath>


namespace BitB
{
  namespace QA
  {

    // Return the cumulative probability from -oo to a normal z value
    static inline double poz(const double z)
    { //{{{

        // Adapted from a polynomial approximation in:
        //  Ibbetson D, Algorithm 209
        //  Collected Algorithms of the CACM 1963 p. 616
        //
        // This routine has six digit accuracy, so it is only useful for
        // absolute z values < 6. For z values >= to 6.0, it returns 0.0.

        static const double Z_MAX = 6.0;

        double y, x, w;

        if (z > 0.0 || z < 0.0) {
            y = 0.5 * fabs(z);
            if (y >= (Z_MAX * 0.5)) {
                x = 1.0;
            } else if (y < 1.0) {
               w = y * y;
               x = ((((((((0.000124818987 * w
                         - 0.001075204047) * w + 0.005198775019) * w
                         - 0.019198292004) * w + 0.059054035642) * w
                         - 0.151968751364) * w + 0.319152932694) * w
                         - 0.531923007300) * w + 0.797884560593) * y * 2.0;
            } else {
                y -= 2.0;
                x = (((((((((((((-0.000045255659 * y
                                + 0.000152529290) * y - 0.000019538132) * y
                                - 0.000676904986) * y + 0.001390604284) * y
                                - 0.000794620820) * y - 0.002034254874) * y
                                + 0.006549791214) * y - 0.010557625006) * y
                                + 0.011630447319) * y - 0.009279453341) * y
                                + 0.005353579108) * y - 0.002141268741) * y
                                + 0.000535310849) * y + 0.999936657524;
            }
        } else {
            x = 0.0;
        }
        return z > 0.0 ? ((x + 1.0) * 0.5) : ((1.0 - x) * 0.5);

    } //}}}

    // Return the probability of chi-squared value x with df degrees of freedom
    static inline double pochisq( double x, unsigned df )
    { //{{{

        // Adapted from:
        //  Hill, I. D. and Pike, M. C.  Algorithm 299
        //  Collected Algorithms for the CACM 1967 p. 243
        //
        // Updated for rounding errors based on remark in ACM TOMS
        // June 1985, page 185

        static const double LOG_SQRT_PI = 0.5723649429247000870717135; /* log(sqrt(pi)) */
        static const double I_SQRT_PI   = 0.5641895835477562869480795; /* 1 / sqrt(pi)  */
        static const double BIGX        = 20.0;        /* max value to represent exp(x) */

        struct local {
            static double ex( double n ) { return n < -BIGX ? 0.0 : exp(n); }
        };

        double a, y, s;
        double e, c, z;
        bool   even;        /* true if df is an even number */

        if (x <= 0.0 || df < 1)
            return 1.0;

        // gcc (up to 4.9.1) is not smart enough to realise y will never
        // be accessed if df==1 and complains about it being uninitialized.
        // It's probably smart enough to optimise this away again though.
        y = 0.0;

        a = 0.5 * x;
        even = (2 * (df / 2)) == df;

        if (df > 1)
            y = local::ex(-a);

        s = (even ? y : (2.0 * poz(-sqrt(x))));

        if (df > 2) {
            x = 0.5 * (df - 1);
            z = (even ? 1.0 : 0.5);

            if (a > BIGX) {
                e = (even ? 0.0 : LOG_SQRT_PI);
                c = log(a);
                while (z <= x) {
                    e = log(z) + e;
                    s += local::ex(c * z - a - e);
                    z += 1.0;
                }
                return s;
            }

            e = (even ? 1.0 : (I_SQRT_PI / sqrt(a)));
            c = 0.0;
            while (z <= x) {
                    e = e * (a / z);
                    c = c + e;
                    z += 1.0;
            }
            return c * y + s;
        }
        return s;

    } //}}}

  } // QA namespace

}   // BitB namespace


#endif  // _BB_CHISQ_H

// vi:sts=4:sw=4:et:foldmethod=marker

