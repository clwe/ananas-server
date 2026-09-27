#include "WfsRenderer.h"
#include "../WFSUtils.h"

namespace ananas::WFS
{
    WfsRenderer::WfsRenderer(juce::AudioProcessorValueTreeState &apvts)
        : state(apvts),
          virtualSourceMessenger(Sockets::VirtualSourceMessengerSocketParams, apvts)
    {
    }

    WfsRenderer::~WfsRenderer()
    {
        if (virtualSourceMessenger.isThreadRunning()) {
            virtualSourceMessenger.stopThread(Constants::WFSMessengerThreadTimeout);
        }
    }

    bool WfsRenderer::drivesModule(const ::ananas::Utils::FirmwareType firmwareType) const
    {
        // Everything except Ambisonics modules plays the source stream, e.g.
        // also passthrough and clock-subscriber firmware.
        return firmwareType != ::ananas::Utils::FirmwareType::ambisonicsModule;
    }

    Server::StreamConfig WfsRenderer::getStreamConfig() const
    {
        return {Server::Sockets::AudioSenderSocketParams, Constants::NumSources};
    }

    void WfsRenderer::prepare(const double sampleRate, const int samplesPerBlock)
    {
        juce::ignoreUnused(sampleRate, samplesPerBlock);
        virtualSourceMessenger.startThread();
    }

    void WfsRenderer::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &stream)
    {
        for (int ch{0}; ch < stream.getNumChannels(); ++ch) {
            if (ch < input.getNumChannels()) {
                stream.copyFrom(ch, 0, input, ch, 0, stream.getNumSamples());
            } else {
                stream.clear(ch, 0, stream.getNumSamples());
            }
        }
    }

    void WfsRenderer::layoutChanged(const ArrayLayout &layout)
    {
        // Virtual source x-coordinates are scaled to the array's width.
        if (!juce::approximatelyEqual(layout.outerSpeakerX, outerSpeakerX)) {
            outerSpeakerX = layout.outerSpeakerX;
            virtualSourceMessenger.setOuterSpeakerX(outerSpeakerX);
            resendVirtualSourcePositions();
        }
    }

    void WfsRenderer::modulesChanged()
    {
        // Newly connected modules need the virtual source positions too.
        resendVirtualSourcePositions();
    }

    void WfsRenderer::resendVirtualSourcePositions()
    {
        for (uint n{0}; n < Constants::NumSources; ++n) {
            const auto idX{Params::getVirtualSourcePositionParamID(n, SourcePositionAxis::X)},
                    idY{Params::getVirtualSourcePositionParamID(n, SourcePositionAxis::Y)};
            virtualSourceMessenger.parameterChanged(idX, state.getRawParameterValue(idX)->load());
            virtualSourceMessenger.parameterChanged(idY, state.getRawParameterValue(idY)->load());
        }
    }
}
