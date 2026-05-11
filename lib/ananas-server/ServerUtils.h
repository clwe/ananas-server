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
         * Capacity, in frames, of the server's FIFO buffer.
         */
        constexpr static uint16_t FifoCapacityFrames{(1 << 10)};

        /**
         * Obsolete... report interval for the FIFO, i.e. how frequently to
         * print the fill-level of the FIFO.
         */
        constexpr static int FifoReportIntervalMs{2000};

        constexpr static size_t ListenerBufferSize{1500};

        constexpr static int PTPFollowUpMessageType{0x08};

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
