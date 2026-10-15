#include <Renderers/DistanceFilter.h>
#include <Renderers/AmbisonicsRenderer.h>

namespace
{
    // The index of the largest absolute sample.
    int peakIndex(const std::vector<float> &samples)
    {
        const auto it = std::max_element(samples.begin(), samples.end(),
                                         [](const float a, const float b) { return std::abs(a) < std::abs(b); });
        return static_cast<int>(std::distance(samples.begin(), it));
    }

    // Processes `samples` in blocks of `blockSize` at a fixed distance.
    void processInBlocks(ananas::Ambisonics::DistanceFilter &filter, std::vector<float> &samples,
                         const int blockSize, const float distance)
    {
        for (size_t start = 0; start < samples.size(); start += static_cast<size_t>(blockSize)) {
            const auto n = std::min(static_cast<size_t>(blockSize), samples.size() - start);
            filter.process(samples.data() + start, static_cast<int>(n), distance);
        }
    }
}

class DistanceFilterTests final : public juce::UnitTest
{
public:
    DistanceFilterTests() : UnitTest("DistanceFilter", "Renderer") {}

    void runTest() override
    {
        using ananas::Ambisonics::DistanceFilter;
        constexpr auto sampleRate = 48000.;

        beginTest("Gain and cutoff follow the WFS prefilter");
        {
            // No boost within a metre.
            expectWithinAbsoluteError(DistanceFilter::computeGain(0.f), 1.f, 1e-6f);
            expectWithinAbsoluteError(DistanceFilter::computeGain(1.f), 1.f, 1e-6f);
            expectWithinAbsoluteError(DistanceFilter::computeGain(4.f), .5f, 1e-6f);
            expectWithinAbsoluteError(DistanceFilter::computeGain(25.f), .2f, 1e-6f);

            expectWithinAbsoluteError(DistanceFilter::computeCutoffHz(1.f), 20000.f, 1e-2f);
            expectWithinAbsoluteError(DistanceFilter::computeCutoffHz(4.f), 12500.f, 1e-2f);
        }

        beginTest("An impulse arrives after d / c");
        {
            DistanceFilter filter;
            filter.prepare(sampleRate);

            const auto distance = 3.43f; // 10 ms
            std::vector<float> samples(2048, 0.f);
            samples[0] = 1.f;
            processInBlocks(filter, samples, 256, distance);

            // The lowpass smears the impulse by a sample or so.
            expectWithinAbsoluteError(peakIndex(samples), 480, 2);
        }

        beginTest("A steady signal is attenuated by the distance gain");
        {
            DistanceFilter filter;
            filter.prepare(sampleRate);

            std::vector<float> samples(4096, 1.f);
            processInBlocks(filter, samples, 512, 4.f);

            expectWithinAbsoluteError(samples.back(), .5f, 1e-3f);
        }

        beginTest("Close up: no delay, no attenuation");
        {
            DistanceFilter filter;
            filter.prepare(sampleRate);

            std::vector<float> samples(1024, 1.f);
            processInBlocks(filter, samples, 128, 0.f);

            expectWithinAbsoluteError(samples.back(), 1.f, 1e-3f);
        }

        beginTest("Distances beyond the maximum are clamped, not out of the delay line");
        {
            DistanceFilter filter;
            filter.prepare(sampleRate);

            std::vector<float> samples(8192, 0.f);
            samples[0] = 1.f;
            processInBlocks(filter, samples, 512, 1000.f);

            const auto expected = static_cast<int>(DistanceFilter::MaxDistanceMetres
                                                   / DistanceFilter::SpeedOfSoundMetresPerSecond * sampleRate);
            expectWithinAbsoluteError(peakIndex(samples), expected, 2);
        }

        beginTest("A distance change ramps over the block, without a jump");
        {
            DistanceFilter filter;
            filter.prepare(sampleRate);

            // A steady signal, so only the gain change shows.
            std::vector<float> first(2048, 1.f);
            processInBlocks(filter, first, 2048, 1.f);
            std::vector<float> second(512, 1.f);
            filter.process(second.data(), static_cast<int>(second.size()), 4.f);

            auto largestStep = std::abs(second.front() - first.back());
            for (size_t i = 1; i < second.size(); ++i) {
                largestStep = std::max(largestStep, std::abs(second[i] - second[i - 1]));
            }
            expect(largestStep < .01f);
            expectWithinAbsoluteError(second.back(), .5f, 1e-2f);
        }

        beginTest("Cues switched off are bypassed");
        {
            using Cues = DistanceFilter::Cues;
            const auto distance = 3.43f; // 10 ms, gain 0.54

            // Delay only: the impulse is late, but at full level and unfiltered.
            DistanceFilter delayOnly;
            delayOnly.prepare(sampleRate);
            std::vector<float> impulse(2048, 0.f);
            impulse[0] = 1.f;
            delayOnly.process(impulse.data(), static_cast<int>(impulse.size()), distance, Cues{true, false, false});
            expectEquals(peakIndex(impulse), 480);
            expectWithinAbsoluteError(impulse[480], 1.f, 1e-4f);

            // Gain only: right away, attenuated.
            DistanceFilter gainOnly;
            gainOnly.prepare(sampleRate);
            std::vector<float> steady(256, 1.f);
            gainOnly.process(steady.data(), static_cast<int>(steady.size()), distance, Cues{false, true, false});
            expectWithinAbsoluteError(steady.front(), DistanceFilter::computeGain(distance), 1e-4f);

            // None: unchanged.
            DistanceFilter none;
            none.prepare(sampleRate);
            std::vector<float> samples{.5f, -.25f, 1.f, 0.f};
            const auto original = samples;
            none.process(samples.data(), static_cast<int>(samples.size()), distance, Cues{false, false, false});
            expect(samples == original);
        }

        beginTest("Distance from the listener in 3-D");
        {
            using ananas::Ambisonics::AmbisonicsRenderer;
            expectWithinAbsoluteError(AmbisonicsRenderer::computeDistance({3.f, 0.f}, 0.f, {0.f, -4.f}), 5.f, 1e-5f);
            expectWithinAbsoluteError(AmbisonicsRenderer::computeDistance({0.f, 3.f}, 4.f, {0.f, 0.f}), 5.f, 1e-5f);
        }
    }
};

static DistanceFilterTests distanceFilterTests;
