#include <LayoutFile.h>

namespace
{
    using ananas::WFS::Speaker;
    namespace LayoutFile = ananas::WFS::LayoutFile;

    ananas::WFS::SpeakerLayout makeLayout(std::vector<Speaker> speakers)
    {
        ananas::WFS::SpeakerLayout layout;
        layout.setSpeakers(std::move(speakers));
        return layout;
    }

    // A file with the given speakers array (as JSON text).
    juce::String fileWithSpeakers(const juce::String &speakers)
    {
        return R"({"format": "ananas-speaker-layout", "version": 1, "units": "metres", "speakers": )" + speakers + "}";
    }
}

class LayoutFileTests final : public juce::UnitTest
{
public:
    LayoutFileTests() : UnitTest("LayoutFile", "Renderer") {}

    void runTest() override
    {
        beginTest("Save and load");
        {
            const auto layout = makeLayout({{{-1.5f, 0.f}, "192.168.10.21", 0},
                                            {{.2f, -.75f}, {}, -1},
                                            {{1.3f, 0.f}, "192.168.10.22", 15}});

            const auto result = LayoutFile::fromJson(LayoutFile::toJson(layout));

            expect(result.wasLoaded());
            expect(result.warnings.isEmpty());
            expect(result.speakers == layout.getSpeakers());
        }

        beginTest("The file has the file-wide settings, and readable numbers");
        {
            const auto json = LayoutFile::toJson(makeLayout({{{.2f, 0.f}, {}, -1}}));
            const auto file = juce::JSON::parse(json);

            expectEquals(file["format"].toString(), juce::String{"ananas-speaker-layout"});
            expectEquals(static_cast<int>(file["version"]), LayoutFile::CurrentVersion);
            expectEquals(file["units"].toString(), juce::String{"metres"});
            expect(file["coordinates"].toString().isNotEmpty());
            // 0.2, not the float's 0.200000003.
            expect(json.contains("0.2") && !json.contains("0.2000"));
            expect(file["speakers"][0].hasProperty("z"));
            expectEquals(file["speakers"][0]["facing"].size(), 3);
            // No module, so no module or output fields.
            expect(!file["speakers"][0].hasProperty("module"));
            expect(!file["speakers"][0].hasProperty("output"));
        }

        beginTest("An empty layout");
        {
            const auto result = LayoutFile::fromJson(LayoutFile::toJson({}));
            expect(result.wasLoaded());
            expect(result.speakers.empty());
        }

        beginTest("Not JSON, not an object, not our format");
        {
            expect(!LayoutFile::fromJson("not json {").wasLoaded());
            expect(!LayoutFile::fromJson("[1, 2]").wasLoaded());
            expect(!LayoutFile::fromJson(R"({"speakers": []})").wasLoaded());
            expect(!LayoutFile::fromJson(R"({"format": "something-else", "version": 1, "speakers": []})").wasLoaded());
        }

        beginTest("A newer version or other units aren't loaded");
        {
            expect(!LayoutFile::fromJson(R"({"format": "ananas-speaker-layout", "speakers": []})").wasLoaded());
            expect(!LayoutFile::fromJson(R"({"format": "ananas-speaker-layout", "version": 2, "speakers": []})").wasLoaded());
            expect(!LayoutFile::fromJson(R"({"format": "ananas-speaker-layout", "version": 1, "units": "feet", "speakers": []})").wasLoaded());
            expect(LayoutFile::fromJson(R"({"format": "ananas-speaker-layout", "version": 1, "speakers": []})").wasLoaded());
        }

        beginTest("A broken speaker: nothing is loaded, and the error names it");
        {
            const auto result = LayoutFile::fromJson(fileWithSpeakers(
                R"([{"x": 0, "y": 0}, {"x": "left", "y": 0}, {"x": 1, "y": 0, "module": "a"}])"));

            expect(!result.wasLoaded());
            expect(result.speakers.empty());
            expectEquals(result.errors.size(), 2);
            expect(result.errors[0].contains("Speaker 2"));
            expect(result.errors[1].contains("Speaker 3"));
        }

        beginTest("Integers and unknown fields");
        {
            const auto result = LayoutFile::fromJson(fileWithSpeakers(
                R"([{"x": 1, "y": -2, "module": "a", "output": 3, "gain": -3.5}])"));

            expect(result.wasLoaded());
            expect(result.warnings.isEmpty());
            expect(result.speakers[0] == Speaker{{1.f, -2.f}, "a", 3});
        }

        beginTest("Height and facing: saved, loaded, and optional");
        {
            using ananas::WFS::Direction;
            const auto tilted = Direction::fromAngles(30.f, -45.f);
            const auto layout = makeLayout({{{0.f, 1.f}, "ambi", 2, 2.5f, tilted}});

            const auto result = LayoutFile::fromJson(LayoutFile::toJson(layout));
            expect(result.wasLoaded());
            expectWithinAbsoluteError(result.speakers[0].z, 2.5f, 1e-4f);
            expectWithinAbsoluteError(result.speakers[0].facing.facingDegrees(), 30.f, .1f);
            expectWithinAbsoluteError(result.speakers[0].facing.tiltDegrees(), -45.f, .1f);

            // Without them: z = 0, facing the listener.
            const auto plain = LayoutFile::fromJson(fileWithSpeakers(R"([{"x": 0, "y": 0}])"));
            expect(plain.speakers[0].facing == Direction{});
            expect(juce::exactlyEqual(plain.speakers[0].z, 0.f));

            // A facing that isn't a unit vector is normalised.
            const auto scaled = LayoutFile::fromJson(fileWithSpeakers(R"([{"x": 0, "y": 0, "facing": [3, 0, 0]}])"));
            expect(scaled.speakers[0].facing == Direction{1.f, 0.f, 0.f});
        }

        beginTest("Broken height or facing: not loaded");
        {
            expect(!LayoutFile::fromJson(fileWithSpeakers(R"([{"x": 0, "y": 0, "z": "high"}])")).wasLoaded());
            expect(!LayoutFile::fromJson(fileWithSpeakers(R"([{"x": 0, "y": 0, "facing": [0, 1]}])")).wasLoaded());
            expect(!LayoutFile::fromJson(fileWithSpeakers(R"([{"x": 0, "y": 0, "facing": [0, 0, 0]}])")).wasLoaded());
            expect(!LayoutFile::fromJson(fileWithSpeakers(R"([{"x": 0, "y": 0, "facing": "front"}])")).wasLoaded());
        }
    }
};

static LayoutFileTests layoutFileTests;
