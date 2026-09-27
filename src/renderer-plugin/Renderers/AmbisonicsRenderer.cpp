#include "AmbisonicsRenderer.h"
#include "AmbisonicEncoder.h"

namespace ananas::Ambisonics
{
    AmbisonicsRenderer::AmbisonicsRenderer(juce::AudioProcessorValueTreeState &apvts, const int firstPassThroughChannel)
        : state(apvts),
          passThroughChannelOffset(firstPassThroughChannel),
          controlMessenger(WFS::Sockets::AmbisonicsControlSocketParams)
    {
        state.addParameterListener(WFS::Params::ListenerX.id, this);
        state.addParameterListener(WFS::Params::ListenerY.id, this);
    }

    AmbisonicsRenderer::~AmbisonicsRenderer()
    {
        state.removeParameterListener(WFS::Params::ListenerX.id, this);
        state.removeParameterListener(WFS::Params::ListenerY.id, this);
        cancelPendingUpdate();
    }

    bool AmbisonicsRenderer::drivesModule(const ::ananas::Utils::FirmwareType firmwareType) const
    {
        return firmwareType == ::ananas::Utils::FirmwareType::ambisonicsModule;
    }

    Server::StreamConfig AmbisonicsRenderer::getStreamConfig() const
    {
        return {WFS::Sockets::AmbisonicsStreamSocketParams, NumChannels};
    }

    void AmbisonicsRenderer::prepare(const double sampleRate, const int samplesPerBlock)
    {
        juce::ignoreUnused(sampleRate, samplesPerBlock);
        for (auto &g: gains) g.fill(0.f);
    }

    juce::Point<float> AmbisonicsRenderer::getListenerMetres() const
    {
        return {
            WFS::Params::positionXToMetres(state.getRawParameterValue(WFS::Params::ListenerX.id)->load(), outerSpeakerX.load()),
            WFS::Params::positionYToMetres(state.getRawParameterValue(WFS::Params::ListenerY.id)->load())
        };
    }

    void AmbisonicsRenderer::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &stream)
    {
        const auto numSamples{stream.getNumSamples()};
        const auto mode{static_cast<int>(state.getRawParameterValue(WFS::Params::AmbisonicsInput.id)->load())};

        if (mode == WFS::Params::AmbisonicsInputPassThrough) {
            for (int ch{0}; ch < stream.getNumChannels(); ++ch) {
                if (passThroughChannelOffset + ch < input.getNumChannels()) {
                    stream.copyFrom(ch, 0, input, passThroughChannelOffset + ch, 0, numSamples);
                } else {
                    stream.clear(ch, 0, numSamples);
                }
            }
            return;
        }

        stream.clear();

        const auto listener{getListenerMetres()};
        const auto outerX{outerSpeakerX.load()};

        for (size_t s{0}; s < NumSources && static_cast<int>(s) < input.getNumChannels(); ++s) {
            const auto index{static_cast<uint>(s)};
            const juce::Point source{
                WFS::Params::positionXToMetres(state.getRawParameterValue(WFS::Params::getVirtualSourcePositionParamID(index, WFS::SourcePositionAxis::X))->load(), outerX),
                WFS::Params::positionYToMetres(state.getRawParameterValue(WFS::Params::getVirtualSourcePositionParamID(index, WFS::SourcePositionAxis::Y))->load())
            };

            std::array<float, NumChannels> target{};
            if (source.getDistanceFrom(listener) < 1e-2f) {
                // At the listening point: no direction; omnidirectional.
                target[0] = 1.f;
            } else {
                computeCoefficients(WFS::Constants::AmbisonicOrder, computeAzimuth(source, listener), 0.f, target.data());
            }

            // Ramp from the previous block's gains to avoid zipper noise when
            // sources or the listener move.
            for (size_t ch{0}; ch < NumChannels; ++ch) {
                stream.addFromWithRamp(static_cast<int>(ch), 0, input.getReadPointer(static_cast<int>(s)), numSamples, gains[s][ch], target[ch]);
            }
            gains[s] = target;
        }
    }

    void AmbisonicsRenderer::layoutChanged(const WFS::ArrayLayout &layout)
    {
        outerSpeakerX = layout.outerSpeakerX;
        speakerPositions = layout.getSpeakerPositions(::ananas::Utils::FirmwareType::ambisonicsModule);
        sendControlMessages(false);
    }

    void AmbisonicsRenderer::modulesChanged()
    {
        // Modules don't store these; (re)connected modules need them.
        sendControlMessages(true);
    }

    void AmbisonicsRenderer::parameterChanged(const juce::String &parameterID, const float newValue)
    {
        juce::ignoreUnused(parameterID, newValue);
        // May be called from the audio thread.
        triggerAsyncUpdate();
    }

    void AmbisonicsRenderer::handleAsyncUpdate()
    {
        sendControlMessages(false);
    }

    float AmbisonicsRenderer::computeRmax(const juce::Point<float> listener, const std::vector<juce::Point<float>> &speakers)
    {
        auto rmax{-1.f};
        for (const auto &s: speakers) {
            rmax = std::max(rmax, s.getDistanceFrom(listener));
        }
        return rmax;
    }

    float AmbisonicsRenderer::computeAzimuth(const juce::Point<float> source, const juce::Point<float> listener)
    {
        const auto d{source - listener};
        return std::atan2(-d.x, d.y);
    }

    void AmbisonicsRenderer::sendControlMessages(const bool force)
    {
        const auto listener{getListenerMetres()};
        controlMessenger.send(listener, computeRmax(listener, speakerPositions), force);
    }
}
