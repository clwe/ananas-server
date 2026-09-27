#ifndef SECONDARYSOURCEMESSENGER_H
#define SECONDARYSOURCEMESSENGER_H

#include <juce_graphics/juce_graphics.h>
#include <juce_osc/juce_osc.h>

namespace ananas::WFS
{
    /**
     * Sends speaker (secondary source) positions, /ss/<j>/x and /ss/<j>/y,
     * unicast to individual modules.
     */
    class SecondarySourceMessenger final : public juce::OSCSender
    {
    public:
        SecondarySourceMessenger();

        /**
         * Send positions for speakers 0 … positions.size() - 1 of one module,
         * as a single OSC bundle.
         */
        bool sendPositions(const juce::String &moduleIP, const std::vector<juce::Point<float>> &positions);

    private:
        juce::DatagramSocket socket;
    };
}

#endif //SECONDARYSOURCEMESSENGER_H
