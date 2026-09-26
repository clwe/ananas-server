#include "Fifo.h"

namespace ananas
{
    Fifo::Fifo(uint8_t numChannels)
        : buffer(std::make_unique<juce::AudioBuffer<float> >(numChannels, Server::Constants::FifoCapacityFrames)),
          converter(std::make_unique<FormatConverter>(numChannels, numChannels))
    {
        startTimer(Server::Constants::FifoReportIntervalMs);
    }

    void Fifo::prepare(const int capacityFrames)
    {
        const std::lock_guard lock{mutex};
        fifo.setTotalSize(capacityFrames);
        buffer->setSize(buffer->getNumChannels(), capacityFrames, false, true, true);
        shouldStop = false;
    }

    bool Fifo::isReady(const int framesRequested) const
    {
        return fifo.getNumReady() >= framesRequested;
    }

    int Fifo::getNumReady() const
    {
        return fifo.getNumReady();
    }

    int64_t Fifo::getNumDroppedFrames() const
    {
        return numDroppedFrames.load();
    }

    void Fifo::write(const juce::AudioBuffer<float> *src)
    {
        // Try to acquire the lock. If the send thread is reading from the FIFO and
        // converting samples, the audio thread will wait here until the send thread
        // is done, which may not be perfectly ideal.
        const std::lock_guard lock{mutex};

        // fifo.write() returns a *scoped* write handle; make sure it goes out of
        // scope — and the number of available samples is updated — before notifying
        // the send thread.
        {
            const auto writeHandle{fifo.write(src->getNumSamples())};
            numDroppedFrames += src->getNumSamples() - (writeHandle.blockSize1 + writeHandle.blockSize2);

            for (auto ch{0}; ch < std::min(buffer->getNumChannels(), src->getNumChannels()); ++ch) {
                const auto readPointer{src->getReadPointer(ch)};
                buffer->copyFrom(ch, writeHandle.startIndex1, readPointer, writeHandle.blockSize1);
                buffer->copyFrom(ch, writeHandle.startIndex2, readPointer + writeHandle.blockSize1, writeHandle.blockSize2);
            }
        }

        // Tell the send thread to check the wait predicate, i.e. see if there are
        // enough samples ready to send.
        condition.notify_one();
    }

    void Fifo::read(uint8_t *dest, const int numFrames)
    {
        // Try to acquire the lock (std::unique_lock because that's what
        // condition_variable::wait() demands). If the audio thread is writing
        // to the FIFO, execution will pause here.
        std::unique_lock lock{mutex};

        // Wait until isReady() returns true. If it returns false, the send thread
        // will relinquish the lock until it is notified, at which point it will
        // try to reacquire the lock and check again.
        // If shouldStop evaluates to true, the plugin is probably being destroyed.
        condition.wait(lock, [this, numFrames]
        {
            return isReady(numFrames) || shouldStop.load();
        });

        // If by this point there aren't actually the requested number of samples
        // available, or, more likely, shouldStop is true, the plugin is probably
        // being destroyed so GTFO.
        if (!isReady(numFrames) || shouldStop.load()) return;

        // The wait predicate passed; read from the FIFO into the destination
        // buffer. NB, the send thread holds the lock until the end of this method;
        // probably fine, but in theory this could block the audio thread.
        const auto readHandle{fifo.read(numFrames)};

        // if (readHandle.blockSize1 + readHandle.blockSize2 != 128) {
        //     // DBG("Read num samples: " << readHandle.blockSize1 + readHandle.blockSize2);
        // }

        const auto blockTwoOffset{readHandle.blockSize1 * buffer->getNumChannels() * sizeof(int16_t)}; // 2 is numChannels...

        for (auto ch{0}; ch < buffer->getNumChannels(); ++ch) {
            converter->convertSamples(dest, ch, buffer->getReadPointer(ch, readHandle.startIndex1), 0, readHandle.blockSize1);
            converter->convertSamples(&dest[blockTwoOffset], ch, buffer->getReadPointer(ch, readHandle.startIndex2), 0, readHandle.blockSize2);
        }
    }

    void Fifo::timerCallback()
    {
#if JUCE_DEBUG
        // std::cout << '\r' << "FIFO size:" << std::setw(6) << fifo.getTotalSize() <<
        //         " | frames ready:" << std::setw(6) << fifo.getNumReady() <<
        //         " | free space:" << std::setw(6) << fifo.getFreeSpace() <<
        //         std::flush;
#endif
    }

    void Fifo::abortRead()
    {
        // Update shouldStop and notify the waiting condition variable. This
        // will terminate a read operation.
        shouldStop = true;
        condition.notify_all();
    }
}
