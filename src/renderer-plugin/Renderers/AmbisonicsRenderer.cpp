#include "AmbisonicsRenderer.h"
#include "AmbisonicEncoder.h"
#include <utility>

namespace ananas::Ambisonics
{
    AmbisonicsRenderer::AmbisonicsRenderer(juce::AudioProcessorValueTreeState &apvts, const int firstPassThroughChannel)
        : state(apvts),
          passThroughChannelOffset(firstPassThroughChannel),
          controlMessenger(WFS::Sockets::AmbisonicsControlSocketParams)
    {
        startTimerHz(ControlMessageRateHz);
    }

    AmbisonicsRenderer::~AmbisonicsRenderer()
    {
        stopTimer();
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
        // In metres already.
        return {
            state.getRawParameterValue(WFS::Params::ListenerX.id)->load(),
            state.getRawParameterValue(WFS::Params::ListenerY.id)->load()
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
        const auto area{sourceArea.load()};

        for (size_t s{0}; s < NumSources && static_cast<int>(s) < input.getNumChannels(); ++s) {
            const auto index{static_cast<uint>(s)};
            const auto source{area.toMetres({
                state.getRawParameterValue(WFS::Params::getVirtualSourcePositionParamID(index, WFS::SourcePositionAxis::X))->load(),
                state.getRawParameterValue(WFS::Params::getVirtualSourcePositionParamID(index, WFS::SourcePositionAxis::Y))->load()
            })};

            const auto z{state.getRawParameterValue(WFS::Params::getVirtualSourcePositionParamID(index, WFS::SourcePositionAxis::Z))->load()};
            const auto horizontal{source.getDistanceFrom(listener)};

            std::array<float, NumChannels> target{};
            if (horizontal < 1e-2f && std::abs(z) < 1e-2f) {
                // At the listening point: no direction; omnidirectional.
                target[0] = 1.f;
            } else {
                computeCoefficients(WFS::Constants::AmbisonicOrder,
                                    computeAzimuth(source, listener),
                                    computeElevation(source, z, listener),
                                    target.data());
            }

            // Ramp from the previous block's gains to avoid zipper noise when
            // sources or the listener move.
            for (size_t ch{0}; ch < NumChannels; ++ch) {
                stream.addFromWithRamp(static_cast<int>(ch), 0, input.getReadPointer(static_cast<int>(s)), numSamples, gains[s][ch], target[ch]);
            }
            gains[s] = target;
        }
    }

    void AmbisonicsRenderer::layoutChanged(const WFS::RenderLayout &layout)
    {
        sourceArea.store(layout.sourceArea);
        speakerPositions.clear();
        speakerHeights.clear();
        for (const auto &s: layout.getSpeakers(::ananas::Utils::FirmwareType::ambisonicsModule)) {
            speakerPositions.push_back(s.position);
            speakerHeights.push_back(s.z);
        }
    }

    void AmbisonicsRenderer::modulesChanged()
    {
        // Modules don't store these; (re)connected modules need them.
        shouldResendControlMessages = true;
    }

    void AmbisonicsRenderer::timerCallback()
    {
        sendControlMessages(std::exchange(shouldResendControlMessages, false));
    }

    float AmbisonicsRenderer::computeRmax(const juce::Point<float> listener, const std::vector<juce::Point<float>> &speakers,
                                          const std::vector<float> &heights)
    {
        auto rmax{-1.f};
        for (size_t i{0}; i < speakers.size(); ++i) {
            const auto horizontal{speakers[i].getDistanceFrom(listener)};
            const auto height{i < heights.size() ? heights[i] : 0.f};
            rmax = std::max(rmax, std::sqrt(horizontal * horizontal + height * height));
        }
        return rmax;
    }

    float AmbisonicsRenderer::computeAzimuth(const juce::Point<float> source, const juce::Point<float> listener)
    {
        const auto d{source - listener};
        return std::atan2(-d.x, d.y);
    }

    float AmbisonicsRenderer::computeElevation(const juce::Point<float> source, const float z, const juce::Point<float> listener)
    {
        return std::atan2(z, source.getDistanceFrom(listener));
    }

    void AmbisonicsRenderer::sendControlMessages(const bool force)
    {
        const auto listener{getListenerMetres()};
        controlMessenger.send(listener, computeRmax(listener, speakerPositions, speakerHeights), force);
    }
}
