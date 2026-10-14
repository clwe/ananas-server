#include <IemLayout.h>

namespace
{
    namespace IemLayout = ananas::WFS::IemLayout;

    // A layout with the given loudspeakers array (as JSON text).
    juce::var layoutWith(const juce::String &loudspeakers)
    {
        return juce::JSON::parse(R"({"Name": "Test", "LoudspeakerLayout": {"Name": "Directions", "Loudspeakers": )"
                                 + loudspeakers + "}}");
    }

    juce::String speaker(const double azimuth, const double elevation, const int channel,
                         const double radius = 1.0, const bool imaginary = false, const double gain = 1.0)
    {
        return "{\"Azimuth\": " + juce::String{azimuth} + ", \"Elevation\": " + juce::String{elevation}
               + ", \"Radius\": " + juce::String{radius} + ", \"IsImaginary\": " + (imaginary ? "true" : "false")
               + ", \"Channel\": " + juce::String{channel} + ", \"Gain\": " + juce::String{gain} + "}";
    }
}

class IemLayoutTests final : public juce::UnitTest
{
public:
    IemLayoutTests() : UnitTest("IemLayout", "Renderer") {}

    void runTest() override
    {
        const juce::Point<float> centre{0.f, -2.f};

        beginTest("Recognised, and other formats not");
        {
            expect(IemLayout::isIemLayout(layoutWith("[]")));
            expect(!IemLayout::isIemLayout(juce::JSON::parse(R"({"format": "ananas-speaker-layout", "speakers": []})")));
            expect(!IemLayout::isIemLayout(juce::var{}));
        }

        beginTest("Directions: front is +y, positive azimuth is left, elevation is up");
        {
            const auto front = IemLayout::fromSpherical(centre, 0.f, 0.f, 2.f);
            expectWithinAbsoluteError(front.position.x, 0.f, 1e-5f);
            expectWithinAbsoluteError(front.position.y, 0.f, 1e-5f);    // 2 m ahead of y = -2

            const auto left = IemLayout::fromSpherical(centre, 90.f, 0.f, 2.f);
            expectWithinAbsoluteError(left.position.x, -2.f, 1e-5f);
            expectWithinAbsoluteError(left.position.y, -2.f, 1e-5f);

            const auto rightFront = IemLayout::fromSpherical(centre, -18.f, 0.f, 1.f);
            expect(rightFront.position.x > 0.f);

            const auto up = IemLayout::fromSpherical(centre, 0.f, 90.f, 2.f);
            expectWithinAbsoluteError(up.z, 2.f, 1e-5f);
            expectWithinAbsoluteError(up.position.y, -2.f, 1e-5f);
        }

        beginTest("Speakers face the centre, tilted down when raised");
        {
            const auto s = IemLayout::fromSpherical(centre, 45.f, 30.f, 3.f);
            // Facing is the opposite of the direction from the centre.
            const auto toCentre = juce::Point<float>{centre.x - s.position.x, centre.y - s.position.y};
            const auto horizontal = std::sqrt(toCentre.x * toCentre.x + toCentre.y * toCentre.y);
            expectWithinAbsoluteError(s.facing.x * horizontal, toCentre.x * s.facing.horizontalLength(), 1e-4f);
            expectWithinAbsoluteError(s.facing.tiltDegrees(), -30.f, 1e-3f);

            const auto front = IemLayout::fromSpherical(centre, 0.f, 0.f, 2.f);
            // The front speaker faces back towards -y: the listener.
            expect(front.facing == ananas::WFS::Direction{}.normalised()
                   || std::abs(front.facing.y + 1.f) < 1e-5f);
        }

        beginTest("Radius scale, unit radii, and sorting by channel");
        {
            const auto json = layoutWith("[" + speaker(18, 0, 2) + "," + speaker(-18, 0, 1) + "]");
            expect(IemLayout::hasUnitRadii(json));

            const auto result = IemLayout::read(json, centre, 2.5f);
            expect(result.wasLoaded());
            expectEquals(static_cast<int>(result.speakers.size()), 2);
            // Channel 1 (azimuth -18, right) first.
            expect(result.speakers[0].position.x > 0.f);
            expectWithinAbsoluteError(result.speakers[0].position.getDistanceFrom(centre), 2.5f, 1e-4f);
            expect(result.speakers[0].moduleIp.isEmpty());

            expect(!IemLayout::hasUnitRadii(layoutWith("[" + speaker(0, 0, 1, 3.0) + "]")));
        }

        beginTest("Imaginary speakers left out; gains and duplicate channels noted");
        {
            const auto result = IemLayout::read(layoutWith("[" + speaker(0, 0, 1) + ","
                                                           + speaker(0, -90, 2, 1, true) + ","
                                                           + speaker(90, 0, 1, 1, false, .5) + "]"),
                                                centre, 2.f);
            expect(result.wasLoaded());
            expectEquals(static_cast<int>(result.speakers.size()), 2);
            expectEquals(result.warnings.size(), 3);
        }

        beginTest("Broken entries and scales are errors");
        {
            expect(!IemLayout::read(layoutWith(R"([{"Azimuth": "left", "Elevation": 0, "Radius": 1}])"), centre, 1.f).wasLoaded());
            expect(!IemLayout::read(layoutWith("[" + speaker(0, 0, 1) + "]"), centre, 0.f).wasLoaded());
            expect(!IemLayout::read(juce::var{}, centre, 1.f).wasLoaded());
        }

        beginTest("The test studio layout: 22 speakers on three rings");
        {
            juce::String speakers{"["};
            const std::vector<std::array<double, 2>> directions{
                {-18, 0}, {18, 0}, {-54, 0}, {54, 0}, {-90, 0}, {90, 0}, {-126, 0}, {126, 0}, {-162, 6}, {162, 0},
                {-22.5, 33}, {22.5, 33}, {-62, 33}, {62, 33}, {-118, 33}, {118, 33}, {-157.5, 33}, {157.5, 33},
                {-45, 66}, {45, 66}, {-135, 66}, {135, 66}};
            for (size_t i = 0; i < directions.size(); ++i) {
                speakers << (i > 0 ? "," : "") << speaker(directions[i][0], directions[i][1], static_cast<int>(i) + 1);
            }
            speakers << "]";

            const auto result = IemLayout::read(layoutWith(speakers), centre, 2.f);
            expect(result.wasLoaded());
            expect(result.warnings.isEmpty());
            expectEquals(static_cast<int>(result.speakers.size()), 22);
            for (const auto &s: result.speakers) {
                const auto horizontal = s.position.getDistanceFrom(centre);
                expectWithinAbsoluteError(std::sqrt(horizontal * horizontal + s.z * s.z), 2.f, 1e-4f);
            }
            // Upper ring at 33 degrees: z = 2 sin(33).
            expectWithinAbsoluteError(result.speakers[10].z, 2.f * std::sin(juce::degreesToRadians(33.f)), 1e-4f);
        }
    }
};

static IemLayoutTests iemLayoutTests;
