#include <LinearArray.h>

namespace
{
    ananas::ModuleList::Entry makeModule(const juce::String &ip, const int numSpeakers)
    {
        ananas::ModuleInfo info;
        info.numSpeakers = numSpeakers;
        return {ip, info, true};
    }

    bool approximatelyEqual(const float a, const float b) { return std::abs(a - b) < 1e-5f; }
}

class LinearArrayTests final : public juce::UnitTest
{
public:
    LinearArrayTests() : UnitTest("LinearArray", "Renderer") {}

    void runTest() override
    {
        using ananas::WFS::generateLinearArray;
        using ananas::WFS::LinearArraySettings;
        constexpr auto legacy = ananas::Utils::Constants::LegacyNumSpeakers;

        beginTest("An empty slot has legacy speakers without a module");
        {
            const auto array = generateLinearArray({1, .2f, {}}, {});
            const auto &speakers = array.layout.getSpeakers();

            expectEquals(static_cast<int>(speakers.size()), legacy);
            for (const auto &s: speakers) {
                expect(s.moduleIp.isEmpty());
                expectEquals(s.output, -1);
            }
        }

        beginTest("A legacy stereo module drives outputs 0 and 1");
        {
            const auto array = generateLinearArray({1, .2f, {"stereo"}}, {makeModule("stereo", legacy)});
            const auto &speakers = array.layout.getSpeakers();

            expectEquals(static_cast<int>(speakers.size()), 2);
            expectEquals(speakers[0].moduleIp, juce::String{"stereo"});
            expectEquals(speakers[0].output, 0);
            expectEquals(speakers[1].moduleIp, juce::String{"stereo"});
            expectEquals(speakers[1].output, 1);
        }

        beginTest("A 16-output module next to an empty slot");
        {
            const auto spacing = .1f;
            const auto array = generateLinearArray({2, spacing, {"adau", ""}}, {makeModule("adau", 16)});
            const auto &speakers = array.layout.getSpeakers();

            expectEquals(static_cast<int>(speakers.size()), 18);

            for (int j = 0; j < 16; ++j) {
                const auto &s = speakers[static_cast<size_t>(j)];
                expectEquals(s.moduleIp, juce::String{"adau"});
                expectEquals(s.output, j);
            }
            for (size_t j = 16; j < 18; ++j) {
                expect(speakers[j].moduleIp.isEmpty());
                expectEquals(speakers[j].output, -1);
            }

            // Left to right, evenly spaced, on the array line.
            for (size_t j = 1; j < speakers.size(); ++j) {
                expect(approximatelyEqual(speakers[j].position.x - speakers[j - 1].position.x, spacing));
                expect(approximatelyEqual(speakers[j].position.y, 0.f));
            }
        }

        beginTest("Slot widths, array width, and an array centred on x = 0");
        {
            const auto spacing = .2f;
            const auto array = generateLinearArray({3, spacing, {"a", "", "b"}},
                                                   {makeModule("a", 16), makeModule("b", 4)});
            const auto &speakers = array.layout.getSpeakers();

            expect(array.slotNumSpeakers == std::vector<int>{16, legacy, 4});
            // Edge to edge: every speaker takes up one spacing.
            expect(approximatelyEqual(array.arrayWidth, 22 * spacing));
            // The outermost speakers are half a spacing inside the edges.
            expect(approximatelyEqual(speakers.front().position.x, -21 * spacing / 2));
            expect(approximatelyEqual(speakers.back().position.x, 21 * spacing / 2));
        }

        beginTest("Positions continue across slots");
        {
            const auto spacing = .2f;
            const auto array = generateLinearArray({2, spacing, {"a", "b"}},
                                                   {makeModule("a", 2), makeModule("b", 2)});
            const auto &speakers = array.layout.getSpeakers();

            // Module b's output 0 comes right after module a's output 1.
            expectEquals(speakers[2].moduleIp, juce::String{"b"});
            expectEquals(speakers[2].output, 0);
            expect(approximatelyEqual(speakers[2].position.x - speakers[1].position.x, spacing));
        }

        beginTest("More IPs than slots: the extra IPs are ignored");
        {
            const auto array = generateLinearArray({1, .2f, {"a", "b"}},
                                                   {makeModule("a", 2), makeModule("b", 16)});

            expect(array.slotNumSpeakers == std::vector<int>{2});
            for (const auto &s: array.layout.getSpeakers()) {
                expectEquals(s.moduleIp, juce::String{"a"});
            }
        }

        beginTest("Fewer IPs than slots: the remaining slots are empty");
        {
            const auto array = generateLinearArray({3, .2f, {"a"}}, {makeModule("a", 4)});
            const auto &speakers = array.layout.getSpeakers();

            expect(array.slotNumSpeakers == std::vector<int>{4, legacy, legacy});
            expectEquals(static_cast<int>(speakers.size()), 4 + 2 * legacy);
            expect(speakers.back().moduleIp.isEmpty());
        }

        beginTest("An unknown IP keeps the IP, with the legacy count");
        {
            const auto array = generateLinearArray({1, .2f, {"offline"}}, {makeModule("other", 16)});
            const auto &speakers = array.layout.getSpeakers();

            expect(array.slotNumSpeakers == std::vector<int>{legacy});
            expectEquals(speakers[0].moduleIp, juce::String{"offline"});
            expectEquals(speakers[0].output, 0);
            expectEquals(speakers[1].output, 1);
        }

        beginTest("No slots: no speakers");
        {
            const auto array = generateLinearArray({0, .2f, {}}, {makeModule("a", 2)});

            expect(array.layout.getSpeakers().empty());
            expect(array.slotNumSpeakers.empty());
            expect(approximatelyEqual(array.arrayWidth, 0.f));
        }
    }
};

static LinearArrayTests linearArrayTests;
