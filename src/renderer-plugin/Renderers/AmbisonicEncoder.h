#ifndef AMBISONICENCODER_H
#define AMBISONICENCODER_H

#include <cstddef>

namespace ananas::Ambisonics
{
    /**
     * Number of channels for an Ambisonic order, (order + 1)^2.
     */
    constexpr size_t getNumChannels(const int order)
    {
        return static_cast<size_t>((order + 1) * (order + 1));
    }

    /**
     * Real spherical harmonics for encoding a plane wave from one direction:
     * ACN channel order, SN3D normalisation (AmbiX), no Condon-Shortley phase.
     *
     * @param order Ambisonic order, 0 or more.
     * @param azimuth Radians, counter-clockwise from the front.
     * @param elevation Radians, upwards from the horizontal plane.
     * @param coefficients getNumChannels(order) values, written in ACN order.
     */
    void computeCoefficients(int order, float azimuth, float elevation, float *coefficients);
}

#endif //AMBISONICENCODER_H
