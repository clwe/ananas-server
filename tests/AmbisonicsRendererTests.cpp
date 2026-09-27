#include <Renderers/AmbisonicsRenderer.h>

class AmbisonicsRendererTests final : public juce::UnitTest
{
public:
    AmbisonicsRendererTests() : UnitTest("AmbisonicsRenderer", "Renderer") {}

    void runTest() override
    {
        using ananas::Ambisonics::AmbisonicsRenderer;
        constexpr auto pi{juce::MathConstants<float>::pi};
        const juce::Point listener{0.f, -2.f};

        beginTest("Azimuth: front is the array's +y, left is its -x");
        {
            // Straight ahead of the listener, behind the array.
            expectWithinAbsoluteError(AmbisonicsRenderer::computeAzimuth({0.f, 3.f}, listener), 0.f, 1e-6f);
            // To the listener's left (the array's -x side): +90 degrees.
            expectWithinAbsoluteError(AmbisonicsRenderer::computeAzimuth({-1.f, -2.f}, listener), pi / 2.f, 1e-6f);
            // To the right: -90 degrees.
            expectWithinAbsoluteError(AmbisonicsRenderer::computeAzimuth({1.f, -2.f}, listener), -pi / 2.f, 1e-6f);
            // Behind the listener.
            expectWithinAbsoluteError(std::abs(AmbisonicsRenderer::computeAzimuth({0.f, -3.f}, listener)), pi, 1e-6f);
            // Front left, 45 degrees.
            expectWithinAbsoluteError(AmbisonicsRenderer::computeAzimuth({-1.f, -1.f}, listener), pi / 4.f, 1e-6f);
        }

        beginTest("rmax is the distance to the farthest speaker");
        {
            expectEquals(AmbisonicsRenderer::computeRmax(listener, {}), -1.f);
            const std::vector<juce::Point<float>> speakers{{-3.f, 0.f}, {0.f, 0.f}, {1.f, 0.f}};
            expectWithinAbsoluteError(AmbisonicsRenderer::computeRmax(listener, speakers), std::sqrt(13.f), 1e-6f);
        }
    }
};

static AmbisonicsRendererTests ambisonicsRendererTests;
