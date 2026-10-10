#include "LinearArray.h"
#include <numeric>

namespace ananas::WFS {

LinearArray generateLinearArray(const LinearArraySettings &settings, const std::vector<ModuleList::Entry> &modules)
{
    LinearArray array;

    const auto numSlots = static_cast<size_t>(juce::jmax(settings.numSlots, 0));
    std::vector<juce::String> slotIps(numSlots);

    for (size_t slot = 0; slot < numSlots; ++slot) {
        if (slot < settings.slotModuleIps.size())
            slotIps[slot] = settings.slotModuleIps[slot];

        // The module's output count if it's known; legacy stereo otherwise
        // (empty slot, or a module that isn't in the list).
        auto numSpeakers = ananas::Utils::Constants::LegacyNumSpeakers;
        for (const auto &m: modules) {
            if (!slotIps[slot].isEmpty() && m.ip == slotIps[slot])
                numSpeakers = m.info.numSpeakers;
        }
        array.slotNumSpeakers.push_back(numSpeakers);
    }

    int totalNumSpeakers = std::accumulate(array.slotNumSpeakers.begin(), array.slotNumSpeakers.end(), 0);
    array.arrayWidth = static_cast<float>(totalNumSpeakers) * settings.speakerSpacing;
    float outerSpeakerX = static_cast<float>(juce::jmax(totalNumSpeakers - 1, 0)) * settings.speakerSpacing / 2.f;

    std::vector<Speaker> speakers;
    int first = 0;
    for (size_t slot = 0; slot < slotIps.size(); ++slot) {
        for(int j = 0; j < array.slotNumSpeakers[slot]; ++j) {
            Speaker spk;
            spk.position = {-outerSpeakerX + float(first +j) * settings.speakerSpacing, 0};
            if (!slotIps[slot].isEmpty()) {
                spk.moduleIp = slotIps[slot];
                spk.output = j;
            }
            speakers.push_back(spk);
        }
        first += array.slotNumSpeakers[slot];
    }
    array.layout.setSpeakers(std::move(speakers));

    return array;
}
}