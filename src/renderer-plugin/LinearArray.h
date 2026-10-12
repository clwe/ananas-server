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

/**
 * The plugin's linear-array settings: the one live instance, changed only
 * through its methods, which keep it valid and notify listeners.
 *
 * Always valid: the number of slots and the spacing are within range,
 * there's one module IP (or an empty string) per slot, and a module is in
 * at most one slot.
 */
class LinearArraySetup final : public juce::ChangeBroadcaster
{
public:
    LinearArraySetup();

    [[nodiscard]] const LinearArraySettings &getSettings() const;

    // Clamped to the allowed range. Slots beyond the new number are dropped.
    void setNumSlots(int numSlots);

    // Clamped to the allowed range.
    void setSpeakerSpacing(float spacing);

    /**
     * Puts a module in a slot. If it's in another slot already, that slot is
     * cleared. An empty IP just clears the slot.
     */
    void assignModule(int slot, const juce::String &moduleIp);

    [[nodiscard]] juce::ValueTree toValueTree() const;

    // Applies the same rules as the setters; ignores children of other types.
    void fromValueTree(const juce::ValueTree &tree);

private:
    // Applies the rules to `newSettings`, and notifies listeners if that
    // changes anything.
    void apply(LinearArraySettings newSettings);

    LinearArraySettings settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearArraySetup)
};
}
#endif //LINEARARRAY_H
