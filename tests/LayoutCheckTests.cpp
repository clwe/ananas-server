#include <LayoutCheck.h>
#include <LinearArray.h>

namespace
{
    using ananas::WFS::LayoutProblem;
    using Type = LayoutProblem::Type;

    ananas::ModuleList::Entry makeModule(const juce::String &ip, const int numSpeakers, const bool isConnected = true)
    {
        ananas::ModuleInfo info;
        info.numSpeakers = numSpeakers;
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
    }
};

static LayoutCheckTests layoutCheckTests;
