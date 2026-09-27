#include "ArrayLayout.h"
#include "WFSUtils.h"
#include <numeric>

namespace ananas::WFS
{
    ArrayLayout ArrayLayout::compute(const std::vector<ModuleList::Entry> &modules, const int numSlots, const float speakerSpacing)
    {
        ArrayLayout layout;

        // Which module (if any) is in each slot.
        std::vector<const ModuleList::Entry *> slotModules(static_cast<size_t>(juce::jmax(numSlots, 0)), nullptr);
        for (const auto &m: modules) {
            if (m.info.slot >= 0 && m.info.slot < numSlots && slotModules[static_cast<size_t>(m.info.slot)] == nullptr) {
                slotModules[static_cast<size_t>(m.info.slot)] = &m;
            }
        }

        for (const auto *m: slotModules) {
            layout.slotNumSpeakers.push_back(m != nullptr ? m->info.numSpeakers : ananas::Utils::Constants::LegacyNumSpeakers);
        }

        layout.totalNumSpeakers = std::accumulate(layout.slotNumSpeakers.begin(), layout.slotNumSpeakers.end(), 0);
        layout.arrayWidth = static_cast<float>(layout.totalNumSpeakers) * speakerSpacing;
        layout.outerSpeakerX = static_cast<float>(juce::jmax(layout.totalNumSpeakers - 1, 0)) * speakerSpacing / 2.f;

        // Unassigned modules sit at the origin.
        for (const auto &m: modules) {
            layout.modulePositions[m.ip] = std::vector<juce::Point<float>>(static_cast<size_t>(m.info.numSpeakers));
        }

        layout.numRenderedSources = static_cast<int>(Constants::NumSources);
        auto firstSpeakerIndex{0};

        for (size_t s{0}; s < slotModules.size(); ++s) {
            const auto numSpeakers{layout.slotNumSpeakers[s]};

            if (const auto *m{slotModules[s]}) {
                auto &positions{layout.modulePositions[m->ip]};
                for (int j{0}; j < numSpeakers; ++j) {
                    positions[static_cast<size_t>(j)] = {
                        -layout.outerSpeakerX + static_cast<float>(firstSpeakerIndex + j) * speakerSpacing,
                        0.f
                    };
                }

                // Only WFS modules render the sources individually; for an
                // Ambisonics module, numSources counts Ambisonic channels.
                if (m->info.firmwareType == ananas::Utils::FirmwareType::wfsModule) {
                    if (m->info.numSources < static_cast<int>(Constants::NumSources)) {
                        ++layout.numLimitingModules;
                    }
                    layout.numRenderedSources = juce::jmin(layout.numRenderedSources, m->info.numSources);
                }
            }

            firstSpeakerIndex += numSpeakers;
        }

        return layout;
    }

    juce::var ArrayLayout::toVar() const
    {
        const auto object{new juce::DynamicObject()};

        juce::Array<juce::var> slots;
        for (const auto n: slotNumSpeakers) {
            slots.add(n);
        }

        object->setProperty(Identifiers::ArraySlotNumSpeakersPropertyID, slots);
        object->setProperty(Identifiers::ArrayWidthPropertyID, arrayWidth);
        object->setProperty(Identifiers::ArrayOuterSpeakerXPropertyID, outerSpeakerX);
        object->setProperty(Identifiers::ArrayNumRenderedSourcesPropertyID, numRenderedSources);
        object->setProperty(Identifiers::ArrayNumLimitingModulesPropertyID, numLimitingModules);

        return object;
    }
}
