#ifndef ARRAYLAYOUT_H
#define ARRAYLAYOUT_H

#include <ClientInfo.h>
#include <juce_graphics/juce_graphics.h>

namespace ananas::WFS
{
    /**
     * Speaker positions for a linear array made of module slots.
     *
     * Slots sit edge to edge, left to right. Each slot is as wide as the
     * number of speakers of the module assigned to it (the legacy count if
     * the slot is empty), and all speakers are spaced evenly. A module's
     * ss/0 is its leftmost speaker.
     */
    struct ArrayLayout
    {
        static ArrayLayout compute(const std::vector<ModuleList::Entry> &modules, int numSlots, float speakerSpacing);

        [[nodiscard]] juce::var toVar() const;

        // Number of speakers in each slot.
        std::vector<int> slotNumSpeakers;
        int totalNumSpeakers{0};
        // Edge to edge, i.e. totalNumSpeakers * speakerSpacing.
        float arrayWidth{0.f};
        // x-coordinate of the rightmost speaker; virtual source x-coordinates
        // are scaled to this.
        float outerSpeakerX{0.f};
        // Sources rendered by every assigned module.
        int numRenderedSources{0};
        // Assigned modules that render fewer sources than the plugin provides.
        int numLimitingModules{0};
        // Speaker positions for every known module. Unassigned modules get
        // (0, 0) for all speakers.
        std::map<juce::String, std::vector<juce::Point<float>>> modulePositions;
    };
}

#endif //ARRAYLAYOUT_H
