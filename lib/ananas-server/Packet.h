#ifndef ANANASPACKET_H
#define ANANASPACKET_H

#include <AnanasUtils.h>
#include <juce_core/juce_core.h>

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

        void setTime(timespec ts);

        [[nodiscard]] int64_t getTime() const;

        [[nodiscard]] long getSleepInterval() const;

    private:
        Header header{};
        uint consecutiveBadTimestampCount{0};
        int64_t nsPerPacket{};
        long nsSleepInterval{};
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
    };
#pragma pack(pop)

    struct AuthorityAnnouncePacket
    {
        juce::uint32 serial;
        juce::uint32 usbFeedbackAccumulator;
        int numUnderruns;
        int numOverflows;
    };
}

#endif //ANANASPACKET_H
