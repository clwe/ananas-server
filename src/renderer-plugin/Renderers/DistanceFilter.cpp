#include "DistanceFilter.h"
#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

namespace ananas::Ambisonics
{
    float DistanceFilter::computeGain(const float distanceMetres)
    {
        return 1.f / std::sqrt(std::max(distanceMetres, 1.f));
    }

    float DistanceFilter::computeCutoffHz(const float distanceMetres)
    {
        return computeGain(distanceMetres) * 15000.f + 5000.f;
    }

    void DistanceFilter::prepare(const double newSampleRate)
    {
        sampleRate = newSampleRate;
        // The longest delay, plus one sample for the interpolation.
        const auto maxDelaySamples{static_cast<int>(std::ceil(MaxDistanceMetres / SpeedOfSoundMetresPerSecond * sampleRate))};
        delayLine.assign(static_cast<size_t>(maxDelaySamples + 2), 0.f);
        reset();
    }

    void DistanceFilter::reset()
    {
        std::fill(delayLine.begin(), delayLine.end(), 0.f);
        writePosition = 0;
        hasPrevious = false;
        lowpassState = 0.f;
    }

    void DistanceFilter::process(float *samples, const int numSamples, const float distanceMetres)
    {
        if (delayLine.empty() || numSamples <= 0) return;

        const auto distance{juce::jlimit(0.f, MaxDistanceMetres, distanceMetres)};
        const auto delaySamples{static_cast<float>(distance / SpeedOfSoundMetresPerSecond * sampleRate)};
        const auto gain{computeGain(distance)};

        if (!hasPrevious) {
            previousDelaySamples = delaySamples;
            previousGain = gain;
            hasPrevious = true;
        }

        // One-pole lowpass; the cutoff changes slowly with distance, so once
        // per block is enough.
        const auto cutoff{std::min(computeCutoffHz(distance), .45f * static_cast<float>(sampleRate))};
        const auto coefficient{1.f - std::exp(-juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(sampleRate))};

        const auto size{static_cast<int>(delayLine.size())};
        const auto maxDelay{static_cast<float>(size - 2)};

        for (int i{0}; i < numSamples; ++i) {
            const auto t{static_cast<float>(i + 1) / static_cast<float>(numSamples)};
            const auto delay{std::min(previousDelaySamples + t * (delaySamples - previousDelaySamples), maxDelay)};
            const auto g{previousGain + t * (gain - previousGain)};

            delayLine[static_cast<size_t>(writePosition)] = samples[i];

            // Read `delay` samples back, between two samples.
            auto readPosition{static_cast<float>(writePosition) - delay};
            if (readPosition < 0.f) readPosition += static_cast<float>(size);
            const auto whole{static_cast<int>(readPosition)};
            const auto fraction{readPosition - static_cast<float>(whole)};
            const auto index{whole % size};
            const auto next{(index + 1) % size};
            const auto delayed{delayLine[static_cast<size_t>(index)] * (1.f - fraction)
                               + delayLine[static_cast<size_t>(next)] * fraction};

            lowpassState += coefficient * (g * delayed - lowpassState);
            samples[i] = lowpassState;

            writePosition = (writePosition + 1) % size;
        }

        previousDelaySamples = delaySamples;
        previousGain = gain;
    }
}
