#include <RenderLayout.h>
#include <LinearArray.h>
#include <WFSUtils.h>

namespace
{
    ananas::ModuleList::Entry makeModule(const juce::String &ip, const int slot, const int numSpeakers,
                                         const int numSources = ananas::Utils::Constants::LegacyNumSources,
                                         const ananas::Utils::FirmwareType type = ananas::Utils::FirmwareType::wfsModule)
    {
        ananas::ModuleInfo info;
        info.slot = slot;
        info.numSpeakers = numSpeakers;
        info.numSources = numSources;
        info.firmwareType = type;
        return {ip, info, true};
    }

    bool approximatelyEqual(const float a, const float b) { return std::abs(a - b) < 1e-5f; }

    // What the processor computes from the module list: the linear array
    // (slots from each module's slot) and the render layout derived from it.
    struct Computed
    {
        ananas::WFS::LinearArray array;
        ananas::WFS::RenderLayout layout;

        [[nodiscard]] int totalNumSpeakers() const { return static_cast<int>(array.layout.getSpeakers().size()); }
    };

    Computed compute(const std::vector<ananas::ModuleList::Entry> &modules, const int numSlots, const float speakerSpacing)
    {
        const auto settings = ananas::WFS::settingsFromModuleSlots(modules, numSlots, speakerSpacing);
        auto array = ananas::WFS::generateLinearArray(settings, modules);
        auto layout = ananas::WFS::RenderLayout::compute(array.layout, modules);
        return {std::move(array), std::move(layout)};
    }
}

class RenderLayoutTests final : public juce::UnitTest
{
public:
    RenderLayoutTests() : UnitTest("RenderLayout", "Renderer") {}

    void runTest() override
    {
        beginTest("All-legacy array matches the original 2-speaker geometry");
        {
            const auto c{compute({makeModule("a", 0, 2), makeModule("b", 3, 2)}, 8, .2f)};
            expectEquals(c.totalNumSpeakers(), 16);
            expect(approximatelyEqual(c.array.arrayWidth, 3.2f));
            // Previously: (numModules - .5) * spacing.
            expect(approximatelyEqual(c.layout.outerSpeakerX, 1.5f));
            expect(approximatelyEqual(c.layout.modulePositions.at("a")[0].x, -1.5f));
            expect(approximatelyEqual(c.layout.modulePositions.at("a")[1].x, -1.3f));
            expect(approximatelyEqual(c.layout.modulePositions.at("b")[0].x, -.3f));
        }

        beginTest("Slots are as wide as their module");
        {
            const auto c{compute({makeModule("legacy", 0, 2), makeModule("adau", 1, 16, 8)}, 3, .1f)};
            expect(c.array.slotNumSpeakers == std::vector<int>{2, 16, 2});
            expectEquals(c.totalNumSpeakers(), 20);
            expect(approximatelyEqual(c.layout.outerSpeakerX, .95f));
            const auto &positions{c.layout.modulePositions.at("adau")};
            expectEquals(static_cast<int>(positions.size()), 16);
            expect(approximatelyEqual(positions[0].x, -.95f + 2 * .1f));
            expect(approximatelyEqual(positions[15].x, -.95f + 17 * .1f));
        }

        beginTest("Unassigned modules, and slots beyond the number of modules, are at the origin");
        {
            const auto c{compute({makeModule("free", -1, 16), makeModule("far", 5, 2)}, 3, .1f)};
            expect(c.layout.modulePositions.at("free")[15] == juce::Point<float>{});
            expect(c.layout.modulePositions.at("far")[0] == juce::Point<float>{});
        }

        beginTest("Only WFS modules limit the number of rendered sources");
        {
            const auto wfsOnly{compute({makeModule("wfs", 0, 16, 4)}, 2, .1f)};
            expectEquals(wfsOnly.layout.numRenderedSources, 4);
            expectEquals(wfsOnly.layout.numLimitingModules, 1);

            // An Ambisonics module's numSources counts Ambisonic channels.
            const auto mixed{compute({
                                                      makeModule("wfs", 0, 16, 8),
                                                      makeModule("ambi", 1, 5, 9, ananas::Utils::FirmwareType::ambisonicsModule)
                                                  }, 2, .1f)};
            expectEquals(mixed.layout.numRenderedSources, static_cast<int>(ananas::WFS::Constants::NumSources));
            expectEquals(mixed.layout.numLimitingModules, 0);
            expectEquals(mixed.totalNumSpeakers(), 21);
            expect(approximatelyEqual(mixed.layout.modulePositions.at("ambi")[0].x, -1.f + 16 * .1f));
        }
    }
};

static RenderLayoutTests renderLayoutTests;
