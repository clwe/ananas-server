#ifndef ANANASFIFO_H
#define ANANASFIFO_H

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include "ServerUtils.h"

using FormatConverter = juce::AudioData::ConverterInstance<
    juce::AudioData::Pointer<
        juce::AudioData::Float32,
        juce::AudioData::NativeEndian,
        juce::AudioData::NonInterleaved,
        juce::AudioData::Const
    >,
    juce::AudioData::Pointer<
        juce::AudioData::Int16,
        juce::AudioData::LittleEndian,
        juce::AudioData::Interleaved,
        juce::AudioData::NonConst
    >
>;

namespace ananas
{
    class Fifo final : juce::Timer
    {
    public:
        explicit Fifo(uint8_t numChannels);

        /**
         * Resize and clear the FIFO, and re-enable reading after abortRead().
         * Not to be called concurrently with write().
         */
        void prepare(int capacityFrames);

        /**
         * Used as a condition_variable predicate.
         * @param framesRequested
         * @return
         */
        [[nodiscard]] bool isReady(int framesRequested) const;

        /**
         * Number of frames waiting to be read.
         */
        [[nodiscard]] int getNumReady() const;

        /**
         * Total number of frames discarded because the FIFO was full.
         */
        [[nodiscard]] int64_t getNumDroppedFrames() const;

        /**
         * Write some samples to the FIFO. Called by the audio thread.
         * @param src
         */
        void write(const juce::AudioBuffer<float> *src);

        /**
         * Read some samples from the FIFO. Called by the network send thread.
         * @param dest
         * @param numFrames
         */
        void read(uint8_t *dest, int numFrames);

        void timerCallback() override;

        void abortRead();

    private:
        juce::AbstractFifo fifo{Server::Constants::FifoCapacityFrames};
        std::unique_ptr<juce::AudioBuffer<float>> buffer;
        std::unique_ptr<FormatConverter> converter;
        std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> shouldStop{false};
        std::atomic<int64_t> numDroppedFrames{0};
    };
}


#endif //ANANASFIFO_H
