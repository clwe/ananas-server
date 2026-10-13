#ifndef CUSTOMLAYOUT_H
#define CUSTOMLAYOUT_H

#include "SpeakerLayout.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace ananas::WFS
{
    /**
     * A layout of explicitly placed speakers, edited one speaker at a time:
     * the plugin's one live instance, changed only through its methods,
     * which keep it valid and notify listeners.
     *
     * Always valid: positions are finite, and a speaker without a module has
     * output -1. Whether the layout makes sense (outputs that exist, no two
     * speakers in one place, ...) is checkLayout()'s job, not this class's:
     * a layout being edited is allowed to be wrong for a while.
     *
     * Methods taking a speaker index ignore indices out of range.
     */
    class CustomLayout final : public juce::ChangeBroadcaster
    {
    public:
        CustomLayout() = default;

        [[nodiscard]] const SpeakerLayout &getLayout() const;

        // Replaces all speakers, e.g. with a generated linear array.
        void setSpeakers(std::vector<Speaker> speakers);

        // Appends a speaker; returns its index.
        int addSpeaker(const Speaker &speaker = {});

        void removeSpeaker(int index);

        void setPosition(int index, juce::Point<float> position);

        // An empty IP disconnects the speaker from any module.
        void setModule(int index, const juce::String &moduleIp, int output);

        [[nodiscard]] juce::ValueTree toValueTree() const;

        // Applies the same rules as the setters; ignores children of other types.
        void fromValueTree(const juce::ValueTree &tree);

    private:
        // Applies the rules to `speakers`, and notifies listeners if that
        // changes anything.
        void apply(std::vector<Speaker> speakers);

        [[nodiscard]] bool isValidIndex(int index) const;

        SpeakerLayout layout;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLayout)
    };
}

#endif //CUSTOMLAYOUT_H
