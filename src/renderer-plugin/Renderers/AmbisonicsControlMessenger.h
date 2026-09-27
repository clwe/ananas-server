#ifndef AMBISONICSCONTROLMESSENGER_H
#define AMBISONICSCONTROLMESSENGER_H

#include <AnanasUtils.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_osc/juce_osc.h>

namespace ananas::Ambisonics
{
    /**
     * Multicasts the listener position (/listener/x, /listener/y) and the
     * reference radius (/ambi/rmax) to all Ambisonics modules.
     */
    class ControlMessenger final : public juce::OSCSender
    {
    public:
        explicit ControlMessenger(const Utils::SenderThreadSocketParams &p);

        /**
         * Send the values if they changed since the last send, or if force.
         * @param listener Metres, in the array's frame.
         * @param rmax Metres; not sent if negative (no speakers yet).
         */
        void send(juce::Point<float> listener, float rmax, bool force);

    private:
        bool connect();

        juce::DatagramSocket socket;
        juce::String ip;
        int localPort, remotePort;
        bool connected{false};
        juce::Point<float> lastListener{std::numeric_limits<float>::quiet_NaN(), 0.f};
        float lastRmax{std::numeric_limits<float>::quiet_NaN()};
    };
}

#endif //AMBISONICSCONTROLMESSENGER_H
