#ifndef AMBISONICSRENDERER_H
#define AMBISONICSRENDERER_H

#include <juce_audio_processors/juce_audio_processors.h>
#include "Renderer.h"
#include "AmbisonicsControlMessenger.h"
#include "../WFSUtils.h"

namespace ananas::Ambisonics
{
    /**
     * Ambisonics: encodes the sources, as seen from the listening point, to an
     * Ambisonic signal (or passes one through from the host), which the
     * modules decode for their own speakers. Sends the modules the listener
     * position and the reference radius.
     */
    class AmbisonicsRenderer final : public Rendering::Renderer,
                                     public juce::AudioProcessorValueTreeState::Listener,
                                     juce::AsyncUpdater
    {
    public:
        /**
         * @param firstPassThroughChannel First input channel of the
         * pass-through (Ambisonics in) bus.
         */
        AmbisonicsRenderer(juce::AudioProcessorValueTreeState &apvts, int firstPassThroughChannel);

        ~AmbisonicsRenderer() override;

        [[nodiscard]] bool drivesModule(::ananas::Utils::FirmwareType firmwareType) const override;

        [[nodiscard]] Server::StreamConfig getStreamConfig() const override;

        void prepare(double sampleRate, int samplesPerBlock) override;

        void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &stream) override;

        void layoutChanged(const WFS::ArrayLayout &layout) override;

        void modulesChanged() override;

        void parameterChanged(const juce::String &parameterID, float newValue) override;

        /**
         * Largest distance from the listener to any of the speakers.
         * @return -1 if there are no speakers.
         */
        static float computeRmax(juce::Point<float> listener, const std::vector<juce::Point<float>> &speakers);

        /**
         * Azimuth of a source as seen from the listener, in radians,
         * counter-clockwise from the front. Both in metres in the array's
         * frame; Ambisonic front is the array's +y, left is its -x.
         */
        static float computeAzimuth(juce::Point<float> source, juce::Point<float> listener);

    private:
        static constexpr size_t NumChannels{WFS::Constants::NumAmbisonicChannels};
        static constexpr size_t NumSources{WFS::Constants::NumSources};

        void handleAsyncUpdate() override;

        [[nodiscard]] juce::Point<float> getListenerMetres() const;

        void sendControlMessages(bool force);

        juce::AudioProcessorValueTreeState &state;
        const int passThroughChannelOffset;
        ControlMessenger controlMessenger;
        std::atomic<float> outerSpeakerX{1.f};
        // Message thread only.
        std::vector<juce::Point<float>> speakerPositions;
        // Audio thread only: the gains used at the end of the previous block.
        std::array<std::array<float, NumChannels>, NumSources> gains{};
    };
}

#endif //AMBISONICSRENDERER_H
