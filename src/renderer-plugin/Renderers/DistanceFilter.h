#ifndef DISTANCEFILTER_H
#define DISTANCEFILTER_H

#include <vector>

namespace ananas::Ambisonics
{
    /**
     * Distance cues for one source, applied before encoding: propagation
     * delay (d / c, so moving sources get Doppler), gain and air absorption.
     * Gain and lowpass follow the WFS firmware's prefilter (faust/wfs.dsp),
     * so a source sounds alike on WFS and Ambisonics modules.
     *
     * Delay and gain ramp from the previous block's values to the new ones
     * over each block; the first block after prepare() or reset() starts at
     * its values, without a ramp.
     */
    class DistanceFilter
    {
    public:
        static constexpr float SpeedOfSoundMetresPerSecond{343.f};
        // Longer distances are treated as this: the farthest a source can be
        // from the listener, both within ±10 m, with the source up to 5 m
        // above the listening plane, is about 28.7 m.
        static constexpr float MaxDistanceMetres{30.f};

        // 1 / sqrt(d), and 1 within a metre (no boost close up).
        static float computeGain(float distanceMetres);

        // Air absorption: 20 kHz within a metre, falling towards 5 kHz.
        static float computeCutoffHz(float distanceMetres);

        // Message thread, or before audio runs: allocates.
        void prepare(double sampleRate);

        void reset();

        /**
         * Audio thread: delays, attenuates and lowpasses `samples` in place.
         * @param distanceMetres From the listener at the end of the block;
         * clamped to 0 ... MaxDistanceMetres.
         */
        void process(float *samples, int numSamples, float distanceMetres);

    private:
        double sampleRate{48000.};
        std::vector<float> delayLine;
        int writePosition{0};
        bool hasPrevious{false};
        float previousDelaySamples{0.f};
        float previousGain{1.f};
        float lowpassState{0.f};
    };
}

#endif //DISTANCEFILTER_H
