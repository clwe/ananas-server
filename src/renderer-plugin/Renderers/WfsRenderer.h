#ifndef WFSRENDERER_H
#define WFSRENDERER_H

#include <juce_audio_processors/juce_audio_processors.h>
#include "Renderer.h"
#include "../VirtualSourceMessenger.h"

namespace ananas::WFS
{
    /**
     * Wave field synthesis: sends the sources as they are, and their positions
     * (/vs) for the modules to render.
     */
    class WfsRenderer final : public Rendering::Renderer
    {
    public:
        explicit WfsRenderer(juce::AudioProcessorValueTreeState &apvts);

        ~WfsRenderer() override;

        [[nodiscard]] bool drivesModule(::ananas::Utils::FirmwareType firmwareType) const override;

        [[nodiscard]] Server::StreamConfig getStreamConfig() const override;

        void prepare(double sampleRate, int samplesPerBlock) override;

        void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &stream) override;

        void layoutChanged(const ArrayLayout &layout) override;

        void modulesChanged() override;

    private:
        void resendVirtualSourcePositions();

        juce::AudioProcessorValueTreeState &state;
        VirtualSourceMessenger virtualSourceMessenger;
        float outerSpeakerX{-1.f};
    };
}

#endif //WFSRENDERER_H
