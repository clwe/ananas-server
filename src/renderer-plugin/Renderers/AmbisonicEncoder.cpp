#include "AmbisonicEncoder.h"
#include <cmath>
#include <vector>

namespace ananas::Ambisonics
{
    namespace
    {
        double factorial(const int n)
        {
            double result{1.};
            for (int i{2}; i <= n; ++i) result *= i;
            return result;
        }
    }

    void computeCoefficients(const int order, const float azimuth, const float elevation, float *coefficients)
    {
        const auto x{std::sin(static_cast<double>(elevation))};
        const auto cosElevation{std::cos(static_cast<double>(elevation))};

        for (int m{0}; m <= order; ++m) {
            // Associated Legendre functions P_l^m(x) for l = m ... order, without
            // the Condon-Shortley phase: P_m^m = (2m - 1)!! cos^m(elevation),
            // P_(m+1)^m = x (2m + 1) P_m^m, then the usual recurrence.
            double pmm{1.};
            for (int i{1}; i <= m; ++i) pmm *= (2 * i - 1) * cosElevation;

            double pPrev{0.}, p{pmm};
            for (int l{m}; l <= order; ++l) {
                if (l == m + 1) {
                    pPrev = p;
                    p = x * (2 * m + 1) * pmm;
                } else if (l > m + 1) {
                    const auto pNext{((2 * l - 1) * x * p - (l + m - 1) * pPrev) / (l - m)};
                    pPrev = p;
                    p = pNext;
                }

                // SN3D normalisation.
                const auto norm{std::sqrt((m == 0 ? 1. : 2.) * factorial(l - m) / factorial(l + m))};
                const auto base{static_cast<size_t>(l * l + l)};

                coefficients[base + static_cast<size_t>(m)] = static_cast<float>(norm * p * std::cos(m * static_cast<double>(azimuth)));
                if (m > 0) {
                    coefficients[base - static_cast<size_t>(m)] = static_cast<float>(norm * p * std::sin(m * static_cast<double>(azimuth)));
                }
            }
        }
    }
}
