#ifndef LINEARARRAY_H
#define LINEARARRAY_H

#include "SpeakerLayout.h"
#include <ClientInfo.h>

namespace ananas::WFS { 

struct LinearArraySettings
{
    int numSlots = 0;
    float speakerSpacing = 0.f;
    std::vector<juce::String> slotModuleIps;   // one per slot, empty = no module
};

struct LinearArray
{
    SpeakerLayout layout;
    std::vector<int> slotNumSpeakers;   // for the UI's slot selectors
    float arrayWidth = 0.f; // calculated from speakerSpacing * totalNumSpeakers
};

LinearArray generateLinearArray(const LinearArraySettings &settings,
                                const std::vector<ModuleList::Entry> &modules);

LinearArraySettings settingsFromModuleSlots(const std::vector<ModuleList::Entry> & modules, int numSlots, float speakerSpacing);
}
#endif //LINEARARRAY_H
