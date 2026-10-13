#ifndef SECONDARYSOURCEMESSENGER_H
#define SECONDARYSOURCEMESSENGER_H

#include <juce_graphics/juce_graphics.h>
#include <juce_osc/juce_osc.h>
#include "RenderLayout.h"

namespace ananas::WFS
{
    /**
     * Sends speaker (secondary source) positions unicast to individual
     * modules: /ss/<j>/x and /ss/<j>/y to modules on the legacy speaker
     * protocol, /ss/id and /ss/<j> (7 floats) to modules on protocol 1
     * (teensy-audiosync docs/firmware-handover-speaker-geometry.md).
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

        /**
         * Speaker protocol 1: send every output of one module as /ss/<j> with
         * x, y, z, facing (x, y, z) and gain (1, or 0 for unused outputs),
         * plus /ss/id, as a single OSC bundle. `setId` must not be 0.
         */
        bool sendGeometry(const juce::String &moduleIP, juce::uint32 setId, const std::vector<OutputGeometry> &outputs);

    private:
        bool sendBundle(const juce::String &moduleIP, const juce::OSCBundle &bundle);

        juce::DatagramSocket socket;
    };
}

#endif //SECONDARYSOURCEMESSENGER_H
