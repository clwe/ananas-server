#include <juce_core/juce_core.h>
#include <Renderers/AmbisonicEncoder.h>

class AmbisonicEncoderTests final : public juce::UnitTest
{
public:
    AmbisonicEncoderTests() : UnitTest("AmbisonicEncoder", "Renderer") {}

    void runTest() override
    {
        using ananas::Ambisonics::computeCoefficients;

        beginTest("Channel counts");
        expectEquals(static_cast<int>(ananas::Ambisonics::getNumChannels(1)), 4);
        expectEquals(static_cast<int>(ananas::Ambisonics::getNumChannels(2)), 9);
        expectEquals(static_cast<int>(ananas::Ambisonics::getNumChannels(3)), 16);

        beginTest("Known directions, first order (W, Y, Z, X)");
        {
            float c[4];
            computeCoefficients(1, 0.f, 0.f, c);                                // front
            expectVector(c, {1.f, 0.f, 0.f, 1.f});
            computeCoefficients(1, juce::MathConstants<float>::halfPi, 0.f, c); // left
            expectVector(c, {1.f, 1.f, 0.f, 0.f});
            computeCoefficients(1, 0.f, juce::MathConstants<float>::halfPi, c); // up
            expectVector(c, {1.f, 0.f, 1.f, 0.f});
        }

        beginTest("Second order matches the explicit SN3D (AmbiX) definitions");
        {
            juce::Random random{42};
            for (int i{0}; i < 100; ++i) {
                const auto az{random.nextFloat() * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::pi};
                const auto el{random.nextFloat() * juce::MathConstants<float>::pi - juce::MathConstants<float>::halfPi};
                const auto s3{std::sqrt(3.f) / 2.f};
                const auto ce{std::cos(el)}, se{std::sin(el)};

                float c[9];
                computeCoefficients(2, az, el, c);
                expectVector(c, {
                                 1.f,
                                 std::sin(az) * ce, se, std::cos(az) * ce,
                                 s3 * ce * ce * std::sin(2 * az),       // V
                                 s3 * std::sin(2 * el) * std::sin(az),  // T
                                 .5f * (3 * se * se - 1.f),             // R
                                 s3 * std::sin(2 * el) * std::cos(az),  // S
                                 s3 * ce * ce * std::cos(2 * az)        // U
                             });
            }
        }

        beginTest("Third order matches the AmbiX definitions for sectoral components");
        {
            float c[16];
            computeCoefficients(3, .3f, 0.f, c);
            // ACN 15 (l = 3, m = 3): sqrt(5/8) cos(3 az) cos^3(el);
            // ACN 9 (l = 3, m = -3): sqrt(5/8) sin(3 az) cos^3(el).
            expectWithinAbsoluteError(c[15], std::sqrt(5.f / 8.f) * std::cos(.9f), 1e-5f);
            expectWithinAbsoluteError(c[9], std::sqrt(5.f / 8.f) * std::sin(.9f), 1e-5f);
        }
    }

private:
    void expectVector(const float *actual, const std::initializer_list<float> expected)
    {
        size_t i{0};
        for (const auto e: expected) {
            expectWithinAbsoluteError(actual[i], e, 1e-5f, "ACN " + juce::String{i});
            ++i;
        }
    }
};

static AmbisonicEncoderTests ambisonicEncoderTests;
