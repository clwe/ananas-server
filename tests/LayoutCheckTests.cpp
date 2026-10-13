#include <LayoutCheck.h>
#include <LinearArray.h>
#include <WFSUtils.h>

namespace
{
    using ananas::WFS::LayoutProblem;
    using Type = LayoutProblem::Type;

    ananas::ModuleList::Entry makeModule(const juce::String &ip, const int numSpeakers, const bool isConnected = true,
                                         const ananas::Utils::FirmwareType type = ananas::Utils::FirmwareType::wfsModule)
    {
        ananas::ModuleInfo info;
        info.numSpeakers = numSpeakers;
        info.firmwareType = type;
        return {ip, info, isConnected};
    }

    ananas::WFS::SpeakerLayout makeLayout(std::vector<ananas::WFS::Speaker> speakers)
    {
        ananas::WFS::SpeakerLayout layout;
        layout.setSpeakers(std::move(speakers));
        return layout;
    }

    // Speakers along y = 0, 0.2 m apart.
    ananas::WFS::Speaker speaker(const int position, const juce::String &ip, const int output)
    {
        return {{.2f * static_cast<float>(position), 0.f}, ip, output};
    }

    bool approximatelyEqual(const float a, const float b) { return std::abs(a - b) < 1e-5f; }

    int count(const std::vector<LayoutProblem> &problems, const Type type)
    {
        return static_cast<int>(std::count_if(problems.begin(), problems.end(),
                                              [type](const LayoutProblem &p) { return p.type == type; }));
    }

    bool has(const std::vector<LayoutProblem> &problems, const Type type, const int speakerIndex)
    {
        return std::any_of(problems.begin(), problems.end(), [&](const LayoutProblem &p)
        {
            return p.type == type && p.speakerIndex == speakerIndex;
        });
    }
}

class LayoutCheckTests final : public juce::UnitTest
{
public:
    LayoutCheckTests() : UnitTest("LayoutCheck", "Renderer") {}

    void runTest() override
    {
        using ananas::WFS::checkLayout;
        using Severity = LayoutProblem::Severity;

        beginTest("A complete layout of connected modules has no problems");
        {
            const auto layout = makeLayout({speaker(0, "a", 0), speaker(1, "a", 1),
                                            speaker(2, "b", 0), speaker(3, "b", 1)});
            const auto problems = checkLayout(layout, {makeModule("a", 2), makeModule("b", 2)});

            expect(problems.empty());
        }

        beginTest("A generated linear array with legacy stereo modules has no warnings");
        {
            const std::vector<ananas::ModuleList::Entry> modules{makeModule("a", 2), makeModule("b", 16)};
            const auto array = ananas::WFS::generateLinearArray({2, .1f, {"a", "b"}}, modules);
            const auto problems = checkLayout(array.layout, modules);

            expect(problems.empty());
        }

        beginTest("Speakers without a module are info, not warnings");
        {
            const auto layout = makeLayout({speaker(0, "a", 0), speaker(1, "a", 1),
                                            speaker(2, {}, -1), speaker(3, {}, -1)});
            const auto problems = checkLayout(layout, {makeModule("a", 2)});

            expectEquals(count(problems, Type::noModule), 2);
            expect(has(problems, Type::noModule, 2));
            for (const auto &p: problems) {
                expect(p.severity == Severity::info);
            }
        }

        beginTest("Unknown and disconnected modules");
        {
            const auto layout = makeLayout({speaker(0, "gone", 0), speaker(1, "off", 0), speaker(2, "off", 1)});
            const auto problems = checkLayout(layout, {makeModule("off", 2, false)});

            expect(has(problems, Type::unknownModule, 0));
            expect(has(problems, Type::moduleNotConnected, 1));
            expect(has(problems, Type::moduleNotConnected, 2));
            expectEquals(count(problems, Type::outputUnused), 0);
        }

        beginTest("An output the module doesn't have");
        {
            const auto layout = makeLayout({speaker(0, "stereo", 0), speaker(1, "stereo", 1),
                                            speaker(2, "stereo", 5), speaker(3, "stereo", -1)});
            const auto problems = checkLayout(layout, {makeModule("stereo", 2)});

            expect(has(problems, Type::outputOutOfRange, 2));
            expect(has(problems, Type::outputOutOfRange, 3));
            expectEquals(count(problems, Type::outputOutOfRange), 2);
        }

        beginTest("A module referenced only by out-of-range outputs: its real outputs are unused");
        {
            const auto layout = makeLayout({speaker(0, "stereo", 5)});
            const auto problems = checkLayout(layout, {makeModule("stereo", 2)});

            expectEquals(count(problems, Type::outputUnused), 2);
        }

        beginTest("An output used twice is marked on both speakers");
        {
            const auto layout = makeLayout({speaker(0, "a", 0), speaker(1, "a", 0), speaker(2, "a", 1)});
            const auto problems = checkLayout(layout, {makeModule("a", 2)});

            expect(has(problems, Type::outputUsedTwice, 0));
            expect(has(problems, Type::outputUsedTwice, 1));
            expectEquals(count(problems, Type::outputUsedTwice), 2);
        }

        beginTest("Speakers at the same position are marked on both");
        {
            const auto layout = makeLayout({speaker(0, "a", 0), {{.005f, 0.f}, "a", 1}, speaker(1, "b", 0),
                                            speaker(2, "b", 1)});
            const auto problems = checkLayout(layout, {makeModule("a", 2), makeModule("b", 2)});

            expect(has(problems, Type::samePosition, 0));
            expect(has(problems, Type::samePosition, 1));
            expectEquals(count(problems, Type::samePosition), 2);
        }

        beginTest("Unused outputs of a module in the layout, not of unassigned modules");
        {
            const auto layout = makeLayout({speaker(0, "adau", 0), speaker(1, "adau", 1)});
            const auto problems = checkLayout(layout, {makeModule("adau", 16), makeModule("free", 2)});

            expectEquals(count(problems, Type::outputUnused), 14);
            for (const auto &p: problems) {
                expectEquals(p.moduleIp, juce::String{"adau"});
                expectEquals(p.speakerIndex, -1);
            }
        }

        beginTest("Problems are ordered by speaker, module-level ones last");
        {
            const auto layout = makeLayout({speaker(0, "a", 0), speaker(1, "gone", 0), speaker(2, {}, -1)});
            const auto problems = checkLayout(layout, {makeModule("a", 2)});

            expect(!problems.empty());
            auto previous = 0;
            for (const auto &p: problems) {
                const auto key = p.speakerIndex < 0 ? std::numeric_limits<int>::max() : p.speakerIndex;
                expect(key >= previous);
                previous = key;
            }
            expect(problems.back().type == Type::outputUnused);
        }

        beginTest("WFS speakers off y = 0 are warned about; other speakers aren't");
        {
            const auto ambisonics = ananas::Utils::FirmwareType::ambisonicsModule;
            const auto layout = makeLayout({{{0.f, 0.f}, "wfs", 0}, {{.2f, -.5f}, "wfs", 1},
                                            {{0.f, -2.f}, "ambi", 0}, {{1.f, -2.f}, "ambi", 1},
                                            {{2.f, -1.f}, {}, -1}});
            const auto problems = checkLayout(layout, {makeModule("wfs", 2), makeModule("ambi", 2, true, ambisonics)});

            expect(has(problems, Type::wfsSpeakerOffArrayLine, 1));
            expectEquals(count(problems, Type::wfsSpeakerOffArrayLine), 1);
        }

        beginTest("WFS range, direction and height");
        {
            using ananas::WFS::Direction;
            const auto ambisonics = ananas::Utils::FirmwareType::ambisonicsModule;
            const auto layout = makeLayout({{{3.f, 0.f}, "wfs", 0},                                   // beyond 2.25 m
                                            {{0.f, 0.f}, "wfs", 1, 0.f, Direction::fromAngles(0.f, 89.f)}, // pointing up
                                            {{.2f, 0.f}, "wfs", 2, 1.f},                              // raised
                                            {{5.f, -3.f}, "ambi", 0, 2.f, Direction{0.f, 0.f, 1.f}}});
            const auto problems = checkLayout(layout, {makeModule("wfs", 3), makeModule("ambi", 1, true, ambisonics)});

            expect(has(problems, Type::wfsSpeakerOutOfRange, 0));
            expect(has(problems, Type::wfsSpeakerNotHorizontal, 1));
            expect(has(problems, Type::wfsHeightIgnored, 2));
            // Ambisonics speakers may be anywhere, at any height and angle.
            for (const auto &p: problems) {
                expect(p.speakerIndex != 3);
            }
            // The height is information, not a warning.
            for (const auto &p: problems) {
                if (p.type == Type::wfsHeightIgnored) expect(p.severity == Severity::info);
            }
        }

        beginTest("Tree for the UI: one child per speaker, with its worst severity and its problems");
        {
            using ananas::WFS::Identifiers;
            const auto layout = makeLayout({speaker(0, "a", 0), speaker(1, {}, -1), speaker(2, "gone", 0)});
            const auto tree = ananas::WFS::toValueTree(layout, checkLayout(layout, {makeModule("a", 2)}));

            expect(tree.hasType(Identifiers::SpeakerLayoutTreeType));
            expectEquals(tree.getNumChildren(), 3);

            const auto ok = tree.getChild(0);
            expectEquals(static_cast<int>(ok[Identifiers::SpeakerIndexPropertyID]), 0);
            expectEquals(ok[Identifiers::SpeakerModuleIpPropertyID].toString(), juce::String{"a"});
            expectEquals(static_cast<int>(ok[Identifiers::SpeakerOutputPropertyID]), 0);
            expectEquals(static_cast<int>(ok[Identifiers::SpeakerSeverityPropertyID]), -1);
            expect(ok[Identifiers::SpeakerProblemsPropertyID].toString().isEmpty());

            const auto noModule = tree.getChild(1);
            expect(approximatelyEqual(static_cast<float>(noModule[Identifiers::SpeakerXPropertyID]), .2f));
            expectEquals(static_cast<int>(noModule[Identifiers::SpeakerSeverityPropertyID]),
                         static_cast<int>(Severity::info));
            expect(noModule[Identifiers::SpeakerProblemsPropertyID].toString().isNotEmpty());

            expectEquals(static_cast<int>(tree.getChild(2)[Identifiers::SpeakerSeverityPropertyID]),
                         static_cast<int>(Severity::warning));

            // Module a's output 1 drives no speaker.
            expect(tree[Identifiers::SpeakerLayoutModuleProblemsPropertyID].toString().contains("output 1"));
        }
    }
};

static LayoutCheckTests layoutCheckTests;
