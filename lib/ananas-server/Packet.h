#ifndef ANANASPACKET_H
#define ANANASPACKET_H

#include <AnanasUtils.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstddef>
#include <limits>

namespace ananas
{
    class AudioPacket : public juce::MemoryBlock
    {
    public:
#pragma pack(push, 1)
        struct Header
        {
            uint16_t sequenceNumber{0};
            int64_t timestamp;
            uint8_t numChannels;
            uint16_t numFrames;
        };
#pragma pack(pop)

        void prepare(uint numChannels, int framesPerPacket, double sampleRate);

        uint8_t *getAudioData();

        void writeHeader();

        /**
         * Check the packet timestamp against PTP time and re-stamp packets if
         * they've been too far off for a while. Called from the audio thread,
         * on every block; the new timestamp is applied by the next
         * writeHeader().
         * @param ptpTimeNs Current PTP time.
         * @param force Re-stamp regardless of the difference, e.g. after the
         * PTP time base changed or audio processing paused.
         */
        void setTime(int64_t ptpTimeNs, bool force);

        [[nodiscard]] int64_t getTime() const;

        /**
         * Duration of the audio in one packet.
         */
        [[nodiscard]] double getDurationNs() const;

    private:
        static constexpr int64_t NoPendingTimestamp{std::numeric_limits<int64_t>::min()};

        Header header{};
        // Set by setTime() (audio thread), applied by writeHeader() (sender thread).
        std::atomic<int64_t> pendingTimestamp{NoPendingTimestamp};
        // Timestamp of the most recent packet, for reading from other threads.
        std::atomic<int64_t> currentTimestamp{0};
        // PTP time at which the packet timestamp was first found to be off;
        // 0 if it isn't (audio thread only).
        int64_t timestampOffSinceNs{0};
        int64_t nsPerPacket{};
        double nsPerPacketRemainder{};
        double timestampRemainder{0};
        double clientBufferDuration{};
    };

#pragma pack(push, 1)
    struct ClientAnnouncePacket
    {
        juce::uint32 serial;
        Utils::FirmwareType firmwareType;
        Utils::VersionNumber firmwareVersion;
        float samplingRate;
        float percentCPU;
        juce::int32 presentationOffsetFrame;
        juce::int64 presentationOffsetNs;
        juce::int32 audioPTPOffsetNs;
        juce::uint8 bufferFillPercent;
        bool ptpLock;
        float secondarySource0x{0.f};
        float secondarySource0y{0.f};
        float secondarySource1x{0.f};
        float secondarySource1y{0.f};
        // Appended in later firmware; older firmware sends a shorter packet
        // without these. 0 = not reported. See Utils::Constants::Legacy*.
        juce::uint8 numSources{0};
        juce::uint8 numSpeakers{0};
    };
#pragma pack(pop)

    static_assert(sizeof(ClientAnnouncePacket) == 52, "ClientAnnouncePacket must match the client firmware's wire format");

    // Size of the announcement sent by firmware that predates numSources/numSpeakers.
    constexpr size_t LegacyClientAnnouncePacketSize{offsetof(ClientAnnouncePacket, numSources)};

    struct AuthorityAnnouncePacket
    {
        juce::uint32 serial;
        juce::uint32 usbFeedbackAccumulator;
        int numUnderruns;
        int numOverflows;
    };
}

#endif //ANANASPACKET_H
