#ifndef VIRTUALSOURCEMESSENGER_H
#define VIRTUALSOURCEMESSENGER_H

#include <AnanasUtils.h>
#include <Server.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include "SourceArea.h"

namespace ananas::WFS
{
    class VirtualSourceMessenger final : public juce::Thread,
                               public juce::OSCSender,
                               public juce::AudioProcessorValueTreeState::Listener,
                               public juce::Timer
    {
    public:
        VirtualSourceMessenger(const ::ananas::Utils::SenderThreadSocketParams &p, juce::AudioProcessorValueTreeState& apvts);

        ~VirtualSourceMessenger();

        bool connect();

        void run() override;

        void parameterChanged(const juce::String &parameterID, float newValue) override;

        /**
         * Set the source area; normalised virtual source positions are
         * mapped onto it.
         */
        void setSourceArea(const SourceArea &area);

        void timerCallback() override;

    private:
        struct ParamSlot
        {
            std::atomic<float> value{0.0f};
            std::atomic<bool> changed{false};
        };

        void runImpl() const;

        juce::DatagramSocket socket;
        juce::String ip;
        juce::uint16 localPort, remotePort;
        bool connected{false};
        std::unordered_map<juce::String, ParamSlot> slots;
        AtomicSourceArea sourceArea;
        juce::AudioProcessorValueTreeState& state;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VirtualSourceMessenger)
    };
} // ananas::WFS

#endif //VIRTUALSOURCEMESSENGER_H
