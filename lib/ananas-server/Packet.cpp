#include "Packet.h"
#include "ServerUtils.h"

namespace ananas
{
    void AudioPacket::prepare(const uint numChannels, const int framesPerPacket, const double sampleRate)
    {
        setSize(sizeof(Header) + numChannels * framesPerPacket * sizeof(int16_t));
        fillWith(0);
        header.numChannels = numChannels;
        header.numFrames = framesPerPacket;

        // Compute the nanosecond packet timestamp interval. This may not be an
        // integer, so calculate the remainder too, so this can be accumulated
        // (somewhat) accurately.
        nsPerPacket = Server::Constants::NSPS * framesPerPacket / static_cast<int>(sampleRate);
        nsPerPacketRemainder = static_cast<double>(Server::Constants::NSPS) * framesPerPacket / static_cast<int>(sampleRate) - static_cast<double>(nsPerPacket);
        clientBufferDuration = getDurationNs() * Server::Constants::ClientPacketBufferSize;

        std::cout << framesPerPacket << "/" << sampleRate << " = " <<
                nsPerPacket << " + " << nsPerPacketRemainder << " ns per packet." << std::endl;
    }

    double AudioPacket::getDurationNs() const
    {
        return static_cast<double>(nsPerPacket) + nsPerPacketRemainder;
    }

    uint8_t *AudioPacket::getAudioData()
    {
        return &static_cast<uint8_t *>(getData())[sizeof(Header)];
    }

    void AudioPacket::writeHeader()
    {
        if (const auto pending{pendingTimestamp.exchange(NoPendingTimestamp)}; pending != NoPendingTimestamp) {
            header.timestamp = pending;
            timestampRemainder = 0;
        }

        ++header.sequenceNumber;
        header.timestamp += nsPerPacket;
        timestampRemainder += nsPerPacketRemainder;
        if (timestampRemainder > 1) {
            header.timestamp += 1;
            timestampRemainder -= 1;
        }
        currentTimestamp.store(header.timestamp);
        copyFrom(&header, 0, sizeof(Header));
    }

    void AudioPacket::setTime(const int64_t ptpTimeNs, const bool force)
    {
        // Set the time a little ahead; a reproduction offset, and something to
        // compensate for the fact that it took a little while for the follow-up
        // message to arrive.
        const auto newTime{ptpTimeNs + Server::Constants::PacketOffsetNs};

        if (force) {
            std::cout << "Resynchronising packet timestamp to " << newTime << std::endl;
            pendingTimestamp.store(newTime);
            timestampOffSinceNs = 0;
            return;
        }

        // If the difference between the new time and the current packet
        // timestamp persistently exceeds what can possibly be available at the
        // client, update the header timestamp. Brief excursions, e.g. host
        // callback jitter, are absorbed by the packet pacing.
        const auto timestampDiff{static_cast<double>(newTime - currentTimestamp.load())};

        if (timestampDiff > clientBufferDuration / 2 || timestampDiff < -clientBufferDuration / 2) {
            if (timestampOffSinceNs == 0) {
                timestampOffSinceNs = ptpTimeNs;
            } else if (ptpTimeNs - timestampOffSinceNs >= Server::Constants::TimestampResyncPersistenceNs) {
                std::cerr << "Packet timestamp off by " << timestampDiff / 1e6 << " ms; resynchronising to " << newTime << std::endl;
                pendingTimestamp.store(newTime);
                timestampOffSinceNs = 0;
            }
        } else {
            timestampOffSinceNs = 0;
        }
    }

    int64_t AudioPacket::getTime() const
    {
        return currentTimestamp.load();
    }

}
