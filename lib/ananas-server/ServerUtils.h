#ifndef SERVERUTILS_H
#define SERVERUTILS_H

#ifndef FRAMES_PER_PACKET
#define FRAMES_PER_PACKET 16
#endif

// 1/60 @ 32 frames
// 1/80 @ 16 frames
#ifndef PRESENTATION_OFFSET
#define PRESENTATION_OFFSET 70
#endif

#include <AnanasUtils.h>
#include <juce_core/juce_core.h>

namespace ananas::Server
{
    class Constants
    {
    public:
        /**
         * The number of nanoseconds in one second.
         */
        constexpr static int64_t NSPS{1'000'000'000};

        /**
         * The number of frames (per-channel samples) to transmit in each
         * outgoing audio packet.
         */
        constexpr static size_t FramesPerPacket{FRAMES_PER_PACKET};

        /**
         * Tweak this value such that clients stay in the middle of their
         * packet buffer.
         * Increase the divider if clients are reporting a lot of available
         * packets; decrease it if they're reporting too few.
         */
        constexpr static int64_t PacketOffsetNs{NSPS / PRESENTATION_OFFSET};

        /**
         * The number of audio packets stored by each client.
         */
        constexpr static size_t ClientPacketBufferSize{50};

        /**
         * Minimum capacity, in frames, of the server's FIFO buffer. As packets
         * are paced at the audio rate, a host block may still be waiting to be
         * sent when the next arrives, so the FIFO is also sized to hold at
         * least FifoCapacityBlocks host blocks.
         */
        constexpr static int FifoCapacityFrames{(1 << 12)};
        constexpr static int FifoCapacityBlocks{4};

        /**
         * Obsolete... report interval for the FIFO, i.e. how frequently to
         * print the fill-level of the FIFO.
         */
        constexpr static int FifoReportIntervalMs{2000};

        constexpr static size_t ListenerBufferSize{1500};

        constexpr static int PTPFollowUpMessageType{0x08};

        /**
         * A Follow_Up message is 44 bytes; preciseOriginTimestamp occupies
         * bytes 34 to 43.
         */
        constexpr static int PTPFollowUpMinSize{44};

        /**
         * Seconds beyond this (2^32, i.e. the year 2106) are treated as
         * invalid; they would also overflow a nanosecond int64.
         */
        constexpr static int64_t PTPMaxSeconds{int64_t{1} << 32};

        /**
         * How far a Follow_Up timestamp may differ from the time predicted by
         * the previous one (plus the time elapsed locally) and still be
         * accepted.
         */
        constexpr static int64_t PTPTimestampToleranceNs{50'000'000};

        /**
         * Number of consecutive, mutually consistent Follow_Up timestamps
         * required to accept a new PTP time base, e.g. after the grandmaster
         * rebooted.
         */
        constexpr static int PTPNewTimebaseCount{3};

        /**
         * If audio processing pauses for longer than this, or four audio
         * blocks if that's longer (e.g. the host stopped playback), packet
         * timestamps are resynchronised as soon as it resumes.
         */
        constexpr static int64_t AudioGapThresholdNs{50'000'000};
        constexpr static int AudioGapThresholdBlocks{4};

        /**
         * Packets are sent evenly spaced at the audio rate. While more than one
         * host block is waiting to be sent (e.g. the host's audio clock runs
         * slightly fast relative to this machine's), the spacing is shortened
         * by this factor to catch up without sending a burst.
         */
        constexpr static double PacketCatchUpIntervalFactor{.5};

        /**
         * Packet timestamps are compared with PTP time on every audio block;
         * if they stay further apart than the tolerance for this long (e.g.
         * because audio stalled, or was lost), packets are re-stamped.
         */
        constexpr static int64_t TimestampResyncPersistenceNs{100'000'000};

        constexpr static int ClientConnectednessCheckIntervalMs{1000};

        constexpr static int AuthorityConnectednessCheckIntervalMs{1000};

        constexpr static int SwitchInspectorRequestTimeoutS{1};
        inline static const juce::StringRef SwitchMonitorPtpPath{"/rest/system/ptp/monitor"};
        inline static const juce::StringRef SwitchDisablePtpPath{"/rest/system/ptp/disable"};
        inline static const juce::StringRef SwitchEnablePtpPath{"/rest/system/ptp/enable"};

        constexpr static uint ThreadConnectSleepIntervalMs{2500};
        constexpr static uint ThreadConnectWaitIterations{ThreadConnectSleepIntervalMs / 100};
        constexpr static uint ThreadConnectWaitIntervalMs{ThreadConnectSleepIntervalMs / ThreadConnectWaitIterations};
    };

    class Threads
    {
    public:
        inline static const Utils::ThreadParams SwitchInspectorThreadParams{
            "Ananas Switch Inspector",
            100
        };
    };

    class Sockets
    {
    public:
        inline static const Utils::SenderThreadSocketParams AudioSenderSocketParams{
            "Ananas Audio Sender",
            100,
            "224.4.224.4",
            49152,
            49152
        };

        inline static const Utils::SenderThreadSocketParams RebootSenderSocketParams{
            "Ananas Reboot Sender",
            100,
            "224.4.224.5",
            49164,
            49164
        };

        inline static const Utils::ListenerThreadSocketParams TimestampListenerSocketParams{
            "Ananas Timestamp Listener",
            150,
            "224.0.1.129",
            320
        };

        inline static const Utils::ListenerThreadSocketParams AuthorityListenerSocketParams{
            "Ananas Authority Listener",
            100,
            "224.4.224.6",
            49172,
            1000
        };

        inline static const Utils::ListenerThreadSocketParams ClientListenerSocketParams{
            "Ananas Client Listener",
            100,
            "224.4.224.6",
            49173,
            1250
        };
    };
}

#endif //SERVERUTILS_H
