#ifndef ANANASSERVER_H
#define ANANASSERVER_H

#include <juce_core/juce_core.h>
#include "ServerUtils.h"
#include "ClientInfo.h"
#include "AuthorityInfo.h"
#include "SwitchInfo.h"
#include "Fifo.h"
#include "Packet.h"

namespace ananas::Server
{
    /**
     * An outgoing multicast audio stream.
     */
    struct StreamConfig
    {
        // Must refer to strings with static lifetime.
        Utils::SenderThreadSocketParams socketParams;
        uint numChannels;
    };

    class Server final : public juce::AudioSource,
                         public juce::ChangeListener,
                         public juce::ChangeBroadcaster
    {
    public:
        /**
         * A server with a single audio stream to Sockets::AudioSenderSocketParams.
         */
        explicit Server(uint numChannelsToSend);

        /**
         * A server with one audio stream per config, all stamped against the
         * same PTP time.
         */
        explicit Server(const std::vector<StreamConfig> &streamConfigs);

        ~Server() override;

        void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

        void releaseResources() override;

        /**
         * Writes the block to stream 0.
         */
        void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;

        /**
         * Call from the audio thread once per block, before writeStream():
         * checks for pauses, restarts and PTP time base changes, and estimates
         * the current PTP time.
         */
        void beginAudioBlock(int numSamples);

        /**
         * Call from the audio thread for each stream after beginAudioBlock().
         * The buffer should have at least as many channels as the stream.
         * Ignored for inactive streams.
         */
        void writeStream(size_t streamIndex, const juce::AudioBuffer<float> &buffer);

        /**
         * An inactive stream isn't fed and sends nothing. Packets are
         * re-stamped when a stream becomes active.
         */
        void setStreamActive(size_t streamIndex, bool shouldBeActive);

        [[nodiscard]] bool isStreamActive(size_t streamIndex) const;

        [[nodiscard]] size_t getNumStreams() const;

        void changeListenerCallback(ChangeBroadcaster *source) override;

        [[nodiscard]] bool isConnected() const;

        ClientList *getClientList();

        ModuleList *getModuleList();

        AuthorityInfo *getAuthority();

        SwitchList *getSwitches();

    private:
        //======================================================================

        class AnanasThread : public juce::Thread,
                             public ChangeBroadcaster
        {
        public:
            explicit AnanasThread(const Utils::ThreadParams &p);

            virtual bool connect() = 0;

            void run() override;

            int getTimeout() const;

            bool isConnected() const;

        protected:
            virtual void runImpl() = 0;

            int timeoutMs{0};
            bool connected{false};
        };

        //======================================================================

        class UDPMulticastThread : public AnanasThread
        {
        public:
            explicit UDPMulticastThread(const Utils::ThreadSocketParams &p);

            ~UDPMulticastThread() override;

            bool connect() override;

        protected:
            void runImpl() override = 0;

            juce::DatagramSocket socket;
            juce::String ip;
            juce::uint16 localPort;
        };

        //======================================================================

        class SenderThread : public UDPMulticastThread
        {
        public:
            explicit SenderThread(const Utils::SenderThreadSocketParams &p);

        protected:
            juce::uint16 remotePort;
        };

        //======================================================================

        class AudioSender final : public SenderThread
        {
        public:
            AudioSender(const Utils::SenderThreadSocketParams &p, Fifo &fifo);

            bool prepare(uint numChannels, int samplesPerBlockExpected, double sampleRate);

            void setPacketTime(int64_t ptpTimeNs, bool force);

            int64_t getPacketTime() const;

            bool stopThread(int timeOutMilliseconds);

        protected:
            void runImpl() override;

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSender);

            Fifo &fifo;
            AudioPacket packet{};
            int audioBlockSamples{0};
        };

        //======================================================================

        class AnnouncementListenerThread : public UDPMulticastThread
        {
        public:
            explicit AnnouncementListenerThread(const Utils::ListenerThreadSocketParams &p);

            bool connect() override;

        protected:
            void runImpl() override;

            virtual void handlePacket() = 0;

            /**
             * Whether to handle only the newest of several queued packets.
             */
            virtual bool shouldHandleLatestPacketOnly() const { return false; }

            uint8_t buffer[Constants::ListenerBufferSize]{};
            // Size of the packet currently in buffer.
            int numBytesRead{0};
            juce::String senderIP{};
            int senderPort{0};
        };

        //======================================================================

        /**
         * A thread to listen out for PTP timestamps.
         */
        class TimestampListener final : public AnnouncementListenerThread
        {
        public:
            struct Timestamp
            {
                // PTP time from the latest accepted Follow_Up.
                int64_t ptpTimeNs{0};
                // Local (steady clock) time at which it was received; 0 if
                // no timestamp has been accepted yet.
                int64_t receiveTimeNs{0};
            };

            explicit TimestampListener(const Utils::ListenerThreadSocketParams &p);

            /**
             * @return true (once) if the PTP time base has changed since the
             * last call, e.g. on the first timestamp or after the grandmaster
             * rebooted.
             */
            bool hasTimebaseChanged();

            /**
             * Safe to call from the audio thread.
             */
            Timestamp getTimestamp() const noexcept;

        protected:
            void handlePacket() override;

            // Follow_Ups are checked against the local time they arrive, so
            // a stale, queued one would look wrong.
            bool shouldHandleLatestPacketOnly() const override { return true; }

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimestampListener);

            void accept(int64_t ptpTimeNs, int64_t receiveTimeNs, bool isNewTimebase);

            // Listener thread only.
            Timestamp current{};
            Timestamp candidate{};
            int candidateCount{0};

            // Published to other threads; the sequence number (odd while
            // writing) lets readers get a consistent pair without locking.
            std::atomic<uint32_t> sequence{0};
            std::atomic<int64_t> publishedPtpTimeNs{0};
            std::atomic<int64_t> publishedReceiveTimeNs{0};
            std::atomic<bool> timebaseChanged{false};
        };

        //======================================================================

        class ClientListener final : public AnnouncementListenerThread
        {
        public:
            ClientListener(const Utils::ListenerThreadSocketParams &p, ClientList &clients, ModuleList &modules);

        protected:
            void handlePacket() override;

        private:
            ClientList &clients;
            ModuleList &modules;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClientListener);
        };

        //======================================================================

        class AuthorityListener final : public AnnouncementListenerThread
        {
        public:
            AuthorityListener(const Utils::ListenerThreadSocketParams &p, AuthorityInfo &authority);

        protected:
            void handlePacket() override;

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuthorityListener);

            AuthorityInfo &authority;
        };

        //======================================================================

        class RebootSender final : public SenderThread
        {
        public:
            RebootSender(const Utils::SenderThreadSocketParams &p, ClientList &clients);

        protected:
            void runImpl() override;

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RebootSender)

            ClientList &clients;
        };

        //======================================================================

        class SwitchInspector final : public AnanasThread
        {
        public:
            SwitchInspector(const Utils::ThreadParams &p, SwitchList &switches);

            bool connect() override;

        protected:
            void runImpl() override;

        private:
            SwitchList &switches;
            juce::ChildProcess curl;
            int curlTimeoutS{0};

            /**
             * E.g. curl -s -k -u admin:emeraude http://192.168.10.1/rest/system/ptp/monitor -d \'{"numbers":"0","once":""}\' -H "content-type: application/json"
             * @param ip
             * @param username
             * @param password
             * @param path
             * @param postData
             * @return
             */
            juce::var curlRequest(const juce::String &ip,
                                  const juce::String &username,
                                  const juce::String &password,
                                  const juce::StringRef &path,
                                  const juce::String &postData);

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchInspector);
        };

        //======================================================================

        struct AudioStream
        {
            explicit AudioStream(const StreamConfig &c) : config(c), fifo(static_cast<uint8_t>(c.numChannels)) {}

            StreamConfig config;
            Fifo fifo;
            // Owned by Server::threads.
            AudioSender *sender{nullptr};
            std::atomic<bool> active{true};
            // Re-stamp this stream's packets on the next block, e.g. when it
            // becomes active.
            std::atomic<bool> resyncOnNextBlock{false};
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Server)

        // Local time of the previous audio block (audio thread only).
        int64_t lastAudioBlockTimeNs{0};
        // Re-stamp all streams' packets on the next audio block, e.g. after
        // the host re-prepared audio (such as on a buffer size change).
        std::atomic<bool> resyncOnNextBlock{false};
        double audioSampleRate{0};
        // Set by beginAudioBlock() for writeStream() (audio thread only).
        bool blockHasPtpTime{false};
        bool blockForcesResync{false};
        int64_t blockPtpTimeNs{0};
        // Declared before threads, which refer to the streams' FIFOs.
        std::vector<std::unique_ptr<AudioStream>> streams;
        TimestampListener *timestampListener{nullptr};
        SwitchList switches;
        ClientList clients;
        ModuleList modules;
        AuthorityInfo authority;
        juce::OwnedArray<AnanasThread> threads;
    };
}


#endif //ANANASSERVER_H
