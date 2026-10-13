#include <RenderLayout.h>
#include <LinearArray.h>
#include <WFSUtils.h>

namespace
{
    // A module, and the slot the test puts it in (-1: none).
    struct PlacedModule
    {
        ananas::ModuleList::Entry entry;
        int slot;
    };

    PlacedModule makeModule(const juce::String &ip, const int slot, const int numSpeakers,
                            const int numSources = ananas::Utils::Constants::LegacyNumSources,
                            const ananas::Utils::FirmwareType type = ananas::Utils::FirmwareType::wfsModule)
    {
        ananas::ModuleInfo info;
        info.numSpeakers = numSpeakers;
        info.numSources = numSources;
        info.firmwareType = type;
        return {{ip, info, true}, slot};
    }

    bool approximatelyEqual(const float a, const float b) { return std::abs(a - b) < 1e-5f; }

    // What the processor computes: the linear array generated from the
    // settings, and the render layout derived from it.
    struct Computed
    {
        ananas::WFS::LinearArray array;
        ananas::WFS::RenderLayout layout;

        [[nodiscard]] int totalNumSpeakers() const { return static_cast<int>(array.layout.getSpeakers().size()); }
    };

    Computed compute(const std::vector<PlacedModule> &placedModules, const int numSlots, const float speakerSpacing)
    {
        ananas::WFS::LinearArraySettings settings;
        settings.numSlots = numSlots;
        settings.speakerSpacing = speakerSpacing;
        settings.slotModuleIps.resize(static_cast<size_t>(numSlots));

        std::vector<ananas::ModuleList::Entry> modules;
        for (const auto &m: placedModules) {
            modules.push_back(m.entry);
            if (m.slot >= 0 && m.slot < numSlots) {
                settings.slotModuleIps[static_cast<size_t>(m.slot)] = m.entry.ip;
            }
        }

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
            expect(approximatelyEqual(c.layout.moduleOutputs.at("a")[0].position.x, -1.5f));
            expect(approximatelyEqual(c.layout.moduleOutputs.at("a")[1].position.x, -1.3f));
            expect(approximatelyEqual(c.layout.moduleOutputs.at("b")[0].position.x, -.3f));
        }

        beginTest("Slots are as wide as their module");
        {
            const auto c{compute({makeModule("legacy", 0, 2), makeModule("adau", 1, 16, 8)}, 3, .1f)};
            expect(c.array.slotNumSpeakers == std::vector<int>{2, 16, 2});
            expectEquals(c.totalNumSpeakers(), 20);
            expect(approximatelyEqual(c.layout.outerSpeakerX, .95f));
            const auto &outputs{c.layout.moduleOutputs.at("adau")};
            expectEquals(static_cast<int>(outputs.size()), 16);
            expect(approximatelyEqual(outputs[0].position.x, -.95f + 2 * .1f));
            expect(approximatelyEqual(outputs[15].position.x, -.95f + 17 * .1f));
        }

        beginTest("Unassigned modules, and slots beyond the number of modules, are at the origin");
        {
            const auto c{compute({makeModule("free", -1, 16), makeModule("far", 5, 2)}, 3, .1f)};
            // At the origin, and not used (muted on modules that support it).
            expect(c.layout.moduleOutputs.at("free")[15] == ananas::WFS::OutputGeometry{});
            expect(c.layout.moduleOutputs.at("far")[0] == ananas::WFS::OutputGeometry{});
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
            expect(approximatelyEqual(mixed.layout.moduleOutputs.at("ambi")[0].position.x, -1.f + 16 * .1f));
        }

        // Custom layouts: speakers placed directly, not generated.
        const auto computeCustom = [](std::vector<ananas::WFS::Speaker> speakers,
                                      const std::vector<ananas::ModuleList::Entry> &modules)
        {
            ananas::WFS::SpeakerLayout layout;
            layout.setSpeakers(std::move(speakers));
            return ananas::WFS::RenderLayout::compute(layout, modules);
        };
        const auto ambisonics = ananas::Utils::FirmwareType::ambisonicsModule;

        beginTest("Source scaling: an Ambisonics ring around a WFS array doesn't widen it");
        {
            const auto layout = computeCustom({{{-1.f, 0.f}, "wfs", 0}, {{1.f, 0.f}, "wfs", 1},
                                               {{-3.f, -2.f}, "ambi", 0}, {{3.f, -2.f}, "ambi", 1}},
                                              {makeModule("wfs", 0, 2).entry, makeModule("ambi", 0, 2, 9, ambisonics).entry});
            expect(approximatelyEqual(layout.outerSpeakerX, 1.f));
        }

        beginTest("Source scaling: speakers without a module count as part of the array");
        {
            const auto layout = computeCustom({{{-1.f, 0.f}, "wfs", 0}, {{2.f, 0.f}, {}, -1}, {{3.f, -2.f}, "ambi", 0}},
                                              {makeModule("wfs", 0, 1).entry, makeModule("ambi", 0, 1, 9, ambisonics).entry});
            expect(approximatelyEqual(layout.outerSpeakerX, 2.f));
        }

        beginTest("Source scaling: with Ambisonics modules only, all speakers count");
        {
            const auto layout = computeCustom({{{-3.f, -2.f}, "ambi", 0}, {{2.5f, -2.f}, "ambi", 1}},
                                              {makeModule("ambi", 0, 2, 9, ambisonics).entry});
            expect(approximatelyEqual(layout.outerSpeakerX, 3.f));
        }

        beginTest("Per output: position, height, facing, and whether a speaker uses it");
        {
            const auto facing = ananas::WFS::Direction::fromAngles(30.f, 10.f);
            const auto layout = computeCustom({{{-1.f, -2.f}, "ambi", 1, 2.f, facing}, {{1.f, -2.f}, "ambi", 0, .5f}},
                                              {makeModule("ambi", 0, 3, 9, ambisonics).entry});
            const auto &outputs = layout.moduleOutputs.at("ambi");
            expectEquals(static_cast<int>(outputs.size()), 3);
            expect(outputs[0] == ananas::WFS::OutputGeometry{{1.f, -2.f}, .5f, {}, true});
            expect(outputs[1] == ananas::WFS::OutputGeometry{{-1.f, -2.f}, 2.f, facing, true});
            expect(outputs[2] == ananas::WFS::OutputGeometry{});

            // The used outputs only, in output order.
            const auto speakers = layout.getSpeakers(ambisonics);
            expectEquals(static_cast<int>(speakers.size()), 2);
            expect(speakers[0] == outputs[0]);
            expect(speakers[1] == outputs[1]);
            expect(ananas::WFS::RenderLayout::getPositions(outputs)
                   == std::vector<juce::Point<float>>{{1.f, -2.f}, {-1.f, -2.f}, {}});
        }

        beginTest("Position limits by speaker protocol");
        {
            auto legacy = makeModule("legacy", 0, 2).entry;
            auto current = makeModule("current", 0, 2).entry;
            current.info.speakerProtocol = 1;
            using ananas::WFS::Constants;

            expect(approximatelyEqual(ananas::WFS::getMaxPositionMetres(legacy.info), Constants::LegacyMaxPositionMetres));
            expect(approximatelyEqual(ananas::WFS::getMaxPositionMetres(current.info), Constants::MaxPositionMetres));

            // The smallest limit of the assigned WFS modules.
            const auto both = computeCustom({{{-1.f, 0.f}, "legacy", 0}, {{1.f, 0.f}, "current", 0}}, {legacy, current});
            expect(approximatelyEqual(both.maxWfsPositionMetres, Constants::LegacyMaxPositionMetres));
            const auto currentOnly = computeCustom({{{1.f, 0.f}, "current", 0}}, {legacy, current});
            expect(approximatelyEqual(currentOnly.maxWfsPositionMetres, Constants::MaxPositionMetres));
        }

        beginTest("Speaker set ids are never 0");
        {
            expectEquals(static_cast<int>(ananas::WFS::nextSpeakerSetId(0)), 1);
            expectEquals(static_cast<int>(ananas::WFS::nextSpeakerSetId(41)), 42);
            expectEquals(static_cast<int>(ananas::WFS::nextSpeakerSetId(std::numeric_limits<juce::uint32>::max())), 1);
        }
    }
};

static RenderLayoutTests renderLayoutTests;
