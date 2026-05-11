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
        // Audio packets will be transmitted in bursts according to the number
        // of frames available in the FIFO. E.g., for a host buffer size of 128
        // frames, and a framesPerPacket value of 32, four packets will be
        // transmitted for each host audio callback. Without a small delay
        // between transmission of consecutive packets, these bursts can be
        // disruptive to reception of PTP packets, client-side.
        nsSleepInterval = nsPerPacket * 1 / 100;

        clientBufferDuration = (static_cast<double>(nsPerPacket) + nsPerPacketRemainder) * Server::Constants::ClientPacketBufferSize;

        std::cout << framesPerPacket << "/" << sampleRate << " = " <<
                nsPerPacket << " + " << nsPerPacketRemainder << " ns per block. " <<
                "Inter-packet sleep interval " << nsSleepInterval << " ns." << std::endl;
    }

    uint8_t *AudioPacket::getAudioData()
    {
        return &static_cast<uint8_t *>(getData())[sizeof(Header)];
    }

    void AudioPacket::writeHeader()
    {
        ++header.sequenceNumber;
        header.timestamp += nsPerPacket;
        timestampRemainder += nsPerPacketRemainder;
        if (timestampRemainder > 1) {
            header.timestamp += 1;
            timestampRemainder -= 1;
        }
        copyFrom(&header, 0, sizeof(Header));
    }

    void AudioPacket::setTime(const timespec ts)
    {
        // Set the time a little ahead; a reproduction offset, and something to
        // compensate for the fact that it took a little while for the follow-up
        // message to arrive.
        const auto newTime{ts.tv_sec * Server::Constants::NSPS + ts.tv_nsec + Server::Constants::PacketOffsetNs};

        // If the difference between the new time and the current packet
        // timestamp exceeds what can possibly be available at the client,
        // update the header timestamp.
        const auto timestampDiff{static_cast<double>(newTime - header.timestamp)};

        // TODO: make the diff available at server level so it can be displayed in UI
        std::stringstream ss;
        ss.imbue(std::locale("en_GB.UTF-8")); // Use system locale
        ss << std::fixed << std::setprecision(0) << timestampDiff;

        // std::cout << "Audio/PTP timestamp diff " << ss.str() << " ns" << std::endl;

        if (timestampDiff > clientBufferDuration / 2 || timestampDiff < -clientBufferDuration / 2) {
            std::cerr << "Timestamp diff is " << std::fixed << timestampDiff << std::endl;

            // Sometimes bad timestamps come in pairs and things subsequently
            // settle down. Allow a couple of bad timestamps before updating the
            // header.
            if (++consecutiveBadTimestampCount >= 3) {
                std::cerr << "... Setting packet timestamp to " << newTime << std::endl;
                header.timestamp = newTime;
                consecutiveBadTimestampCount = 0;
            }
        }
    }

    int64_t AudioPacket::getTime() const
    {
        return header.timestamp;
    }

    long AudioPacket::getSleepInterval() const
    {
        return nsSleepInterval;
    }
}
