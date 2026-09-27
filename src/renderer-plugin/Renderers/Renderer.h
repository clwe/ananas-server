#ifndef RENDERER_H
#define RENDERER_H

#include <Server.h>
#include "../ArrayLayout.h"

namespace ananas::Rendering
{
    /**
     * A rendering technique: produces one audio stream for the modules that
     * implement it, and sends them what else they need (control messages).
     */
    class Renderer
    {
    public:
        virtual ~Renderer() = default;

        /**
         * Whether modules running this firmware play this renderer's stream.
         */
        [[nodiscard]] virtual bool drivesModule(Utils::FirmwareType firmwareType) const = 0;

        [[nodiscard]] virtual Server::StreamConfig getStreamConfig() const = 0;

        virtual void prepare(double sampleRate, int samplesPerBlock) = 0;

        /**
         * Audio thread: fill the stream buffer (getStreamConfig().numChannels
         * channels) from the plugin's input.
         */
        virtual void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &stream) = 0;

        /**
         * Message thread: the array layout (slots, speaker positions) changed.
         */
        virtual void layoutChanged(const WFS::ArrayLayout &layout) = 0;

        /**
         * Message thread: modules connected, disconnected or changed; resend
         * the control state, which modules don't store.
         */
        virtual void modulesChanged() = 0;
    };
}

#endif //RENDERER_H
