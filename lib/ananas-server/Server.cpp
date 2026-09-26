#include <arpa/inet.h>
#include "Server.h"
#include <AnanasUtils.h>
#include <AuthorityInfo.h>
#include <chrono>
#include <thread>
#include <cstring>

namespace ananas::Server
{
    namespace
    {
        int64_t getSteadyTimeNs()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        }
    }

    Server::Server(const uint numChannelsToSend) : numChannels(numChannelsToSend),
                                                   fifo(numChannelsToSend)
    {
        // Add all the threads.
        threads.add(new AudioSender(Sockets::AudioSenderSocketParams, fifo));
        threads.add(new TimestampListener(Sockets::TimestampListenerSocketParams));
        threads.add(new ClientListener(Sockets::ClientListenerSocketParams, clients, modules));
        threads.add(new AuthorityListener(Sockets::AuthorityListenerSocketParams, authority));
        threads.add(new RebootSender(Sockets::RebootSenderSocketParams, clients));
        threads.add(new SwitchInspector(Threads::SwitchInspectorThreadParams, switches));

        // The server should listen for change messages sent by all threads.
        for (const auto &t: threads) {
            t->addChangeListener(this);
        }
    }

    Server::~Server()
    {
        releaseResources();

        for (const auto &t: threads) {
            t->removeChangeListener(this);
        }
    }

    void Server::prepareToPlay(const int samplesPerBlockExpected, const double sampleRate)
    {
        lastAudioBlockTimeNs = 0;
        audioSampleRate = sampleRate;

        fifo.prepare(std::max(Constants::FifoCapacityFrames, Constants::FifoCapacityBlocks * samplesPerBlockExpected));

        for (const auto &t: threads) {
            if (auto *s = dynamic_cast<AudioSender *>(t)) {
                // The audio sender needs to be prepared; other threads do not.
                s->prepare(numChannels, samplesPerBlockExpected, sampleRate);
            }
            // With the audio sender thread prepared, and memory allocated to
            // its AudioPacket member, it's safe to start all the threads.
            t->startThread();
        }
    }

    void Server::releaseResources()
    {
        for (const auto &t: threads) {
            if (t->isThreadRunning()) {
                if (auto *s = dynamic_cast<AudioSender *>(t)) {
                    // The sender thread overrides stopThread, so handle it
                    // separately.
                    s->stopThread(t->getTimeout());
                } else {
                    // Stop all the other threads as normal.
                    t->stopThread(t->getTimeout());
                }
            }
        }
    }

    void Server::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill)
    {
        // These checks are kind of overkill, since the order in which threads
        // were added to the OwnedArray is known, but as a santiy check...
        const auto nowNs{getSteadyTimeNs()};
        const auto timeSinceLastBlockNs{lastAudioBlockTimeNs > 0 ? nowNs - lastAudioBlockTimeNs : 0};
        lastAudioBlockTimeNs = nowNs;

        if (auto *s = dynamic_cast<AudioSender *>(threads[0])) {
            if (auto *t = dynamic_cast<TimestampListener *>(threads[1])) {
                const auto timebaseChanged{t->hasTimebaseChanged()};
                const auto newTimestampAvailable{t->isNewTimestampAvailable()};
                const auto blockDurationNs{
                    audioSampleRate > 0 ? static_cast<int64_t>(bufferToFill.numSamples * Constants::NSPS / audioSampleRate) : 0
                };
                const auto gapThresholdNs{std::max(Constants::AudioGapThresholdNs, Constants::AudioGapThresholdBlocks * blockDurationNs)};
                const auto resumedAfterGap{timeSinceLastBlockNs > gapThresholdNs};

                if (timebaseChanged || resumedAfterGap) {
                    // Packet timestamps are no longer continuous with PTP time,
                    // so re-stamp now rather than waiting for several bad
                    // Follow_Ups. Estimate the current PTP time from the latest
                    // Follow_Up and the time elapsed since it arrived.
                    if (const auto ts{t->getTimestamp()}; ts.receiveTimeNs > 0) {
                        if (resumedAfterGap) {
                            std::cout << "Audio resumed after " << timeSinceLastBlockNs / 1'000'000 << " ms." << std::endl;
                        }
                        s->setPacketTime(ts.ptpTimeNs + (nowNs - ts.receiveTimeNs), true);
                    }
                } else if (newTimestampAvailable) {
                    // Send the new timestamp to the audio sender to see whether
                    // the packet timestamp needs to be updated.
                    s->setPacketTime(t->getTimestamp().ptpTimeNs, false);
                }
            }
        }

        fifo.write(bufferToFill.buffer);
    }

    void Server::changeListenerCallback(ChangeBroadcaster *source)
    {
        // If one of the threads announces a change, broadcast that change to
        // any listeners.
        sendChangeMessage();
    }

    bool Server::isConnected() const
    {
        return std::all_of(threads.begin(), threads.end(), [](const AnanasThread *t)
        {
            return t->isConnected();
        });
    }

    ClientList *Server::getClientList()
    {
        return &clients;
    }

    ModuleList *Server::getModuleList()
    {
        return &modules;
    }

    AuthorityInfo *Server::getAuthority()
    {
        return &authority;
    }

    SwitchList *Server::getSwitches()
    {
        return &switches;
    }


    //==========================================================================

    Server::AnanasThread::AnanasThread(const Utils::ThreadParams &p)
        : Thread(p.name), timeoutMs(p.timeoutMs)
    {
    }

    void Server::AnanasThread::run()
    {
        while (!connect() && !threadShouldExit()) {
            for (uint i{0}; i < Constants::ThreadConnectWaitIterations && !threadShouldExit(); ++i)
                wait(Constants::ThreadConnectWaitIntervalMs);
        }

        if (!threadShouldExit()) {
            connected = true;
            runImpl();
        }
    }

    int Server::AnanasThread::getTimeout() const
    {
        return timeoutMs;
    }

    bool Server::AnanasThread::isConnected() const
    {
        return connected;
    }

    //==========================================================================

    Server::UDPMulticastThread::UDPMulticastThread(const Utils::ThreadSocketParams &p)
        : AnanasThread(p),
          ip(p.ip),
          localPort(p.localPort)
    {
    }

    Server::UDPMulticastThread::~UDPMulticastThread()
    {
        socket.leaveMulticast(ip);
        socket.shutdown();
    }

    bool Server::UDPMulticastThread::connect()
    {
        if (-1 == socket.getBoundPort()) {
            if (!socket.setEnablePortReuse(true)) {
                std::cerr << getThreadName() << " failed to set socket port reuse: " << strerror(errno) << std::endl;
                sendChangeMessage();
                return false;
            }

            if (!socket.bindToPort(localPort, Utils::Strings::LocalInterfaceIP)) {
                std::cerr << getThreadName() << " failed to bind socket to port: " << strerror(errno) << std::endl;
                sendChangeMessage();
                return false;
            }

            if (!socket.joinMulticast(ip)) {
                std::cerr << getThreadName() << " failed to join multicast group: " << strerror(errno) << std::endl;
                sendChangeMessage();
                return false;
            }

            socket.setMulticastLoopbackEnabled(false);
            socket.waitUntilReady(false, 1000);
        }

        sendChangeMessage();
        return true;
    }

    //==========================================================================

    Server::SenderThread::SenderThread(const Utils::SenderThreadSocketParams &p)
        : UDPMulticastThread(p),
          remotePort(p.remotePort)
    {
    }

    //==========================================================================

    Server::AudioSender::AudioSender(const Utils::SenderThreadSocketParams &p, Fifo &fifo)
        : SenderThread(p),
          fifo(fifo)
    {
    }

    bool Server::AudioSender::prepare(const uint numChannels, const int samplesPerBlockExpected, const double sampleRate)
    {
        juce::ignoreUnused(sampleRate);

        audioBlockSamples = samplesPerBlockExpected;

        packet.prepare(numChannels, Constants::FramesPerPacket, sampleRate);

        return startThread();
    }

    void Server::AudioSender::setPacketTime(const int64_t ptpTimeNs, const bool force)
    {
        packet.setTime(ptpTimeNs, force);
    }

    int64_t Server::AudioSender::getPacketTime() const
    {
        return packet.getTime();
    }

    bool Server::AudioSender::stopThread(const int timeOutMilliseconds)
    {
        fifo.abortRead();
        return Thread::stopThread(timeOutMilliseconds);
    }

    void Server::AudioSender::runImpl()
    {
        std::cout << getThreadName() << " sending audio packets..." << std::endl << std::flush;

        // Send packets evenly spaced at the audio rate, rather than each host
        // block as a burst. Clients drain their Ethernet receive ring (only a
        // few frames deep) from their main loop, which gets little time while
        // their DSP runs; a burst of packets overflows it and packets are lost,
        // e.g. 16 packets per host block at 256 frames.
        const auto packetDurationNs{packet.getDurationNs()};
        auto nextSendTimeNs{static_cast<double>(getSteadyTimeNs())};
        auto numDroppedFramesReported{fifo.getNumDroppedFrames()};
        auto lastDropReportTimeNs{0.};

        while (!threadShouldExit()) {
            // Read from the fifo into the packet.
            fifo.read(packet.getAudioData(), Constants::FramesPerPacket);
            if (threadShouldExit()) break;
            // Write the header to the packet.
            packet.writeHeader();

            const auto nowNs{static_cast<double>(getSteadyTimeNs())};

            if (nowNs - nextSendTimeNs > packetDurationNs) {
                // Behind schedule, e.g. audio arrived late or after a pause;
                // restart the schedule rather than catching up in a burst.
                nextSendTimeNs = nowNs;
            } else if (nowNs < nextSendTimeNs) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<int64_t>(nextSendTimeNs - nowNs)));
            }

            // Write the packet to the socket.
            socket.write(ip, remotePort, packet.getData(), static_cast<int>(packet.getSize()));

            if (const auto dropped{fifo.getNumDroppedFrames()}; dropped != numDroppedFramesReported &&
                                                               nowNs - lastDropReportTimeNs > Constants::NSPS) {
                std::cerr << "Audio FIFO full: " << dropped - numDroppedFramesReported << " frames discarded." << std::endl;
                numDroppedFramesReported = dropped;
                lastDropReportTimeNs = nowNs;
            }

            const auto isBacklogged{fifo.getNumReady() > audioBlockSamples + static_cast<int>(Constants::FramesPerPacket)};
            nextSendTimeNs += packetDurationNs * (isBacklogged ? Constants::PacketCatchUpIntervalFactor : 1.);
        }

        std::cout << getThreadName() << " stopping." << std::endl;
    }

    //==========================================================================

    Server::AnnouncementListenerThread::AnnouncementListenerThread(
        const Utils::ListenerThreadSocketParams &p
    ) : UDPMulticastThread(p)
    {
    }

    bool Server::AnnouncementListenerThread::connect()
    {
        if (-1 == socket.getBoundPort()) {
            // JUCE doesn't handle multicast in a manner that's compatible with
            // reading multicast packets on a specific interface...

            // ...(it's probably not necessary to allow port-reuse, but what the
            // hell)...
            if (!socket.setEnablePortReuse(true)) {
                std::cerr << getThreadName() << " failed to set socket port reuse: " << strerror(errno) << std::endl;
                sendChangeMessage();
                return false;
            }

            // ...bind the relevant port to ALL interfaces (INADDR_ANY) by not
            // specifying a local interface here...
            if (!socket.bindToPort(localPort)) {
                std::cerr << getThreadName() << " failed to bind socket to port: " << strerror(errno) << std::endl;
                sendChangeMessage();
                return false;
            }

            // ...then join the appropriate multicast group on the relevant
            // interface (i.e. a manually-configured ethernet connection).
            ip_mreq mreq{};
            mreq.imr_multiaddr.s_addr = inet_addr(ip.toRawUTF8());
            mreq.imr_interface.s_addr = inet_addr(Utils::Strings::LocalInterfaceIP.text);

            if (setsockopt(
                    socket.getRawSocketHandle(),
                    IPPROTO_IP,
                    IP_ADD_MEMBERSHIP,
                    &mreq, sizeof (mreq)) < 0) {
                std::cerr << getThreadName() << " failed to add multicast membership: " << strerror(errno) << std::endl;
                sendChangeMessage();
                return false;
            }

            socket.setMulticastLoopbackEnabled(false);
        }

        sendChangeMessage();
        return true;
    }

    void Server::AnnouncementListenerThread::runImpl()
    {
        std::cout << getThreadName() << " listening..." << std::endl << std::flush;

        while (!threadShouldExit()) {
            if (socket.waitUntilReady(true, timeoutMs)) {
                if (threadShouldExit()) break;

                auto bytesRead{socket.read(buffer, Constants::ListenerBufferSize, false, senderIP, senderPort)};

                // If packets have queued up (e.g. the thread was held up),
                // optionally skip to the newest one.
                while (bytesRead > 0 && shouldHandleLatestPacketOnly() && socket.waitUntilReady(true, 0)) {
                    const auto nextBytesRead{socket.read(buffer, Constants::ListenerBufferSize, false, senderIP, senderPort)};
                    if (nextBytesRead <= 0) break;
                    bytesRead = nextBytesRead;
                }

                if (bytesRead > 0) {
                    numBytesRead = bytesRead;
                    handlePacket();
                } else if (bytesRead < 0) {
                    std::cerr << getThreadName() << ": error reading from socket: " << strerror(errno) << std::endl;
                }
            }
        }

        std::cout << getThreadName() << " stopping." << std::endl;
    }

    //==============================================================================

    Server::TimestampListener::TimestampListener(
        const Utils::ListenerThreadSocketParams &p
    ) : AnnouncementListenerThread(p)
    {
    }

    bool Server::TimestampListener::isNewTimestampAvailable()
    {
        return newTimestampAvailable.exchange(false, std::memory_order_acquire);
    }

    bool Server::TimestampListener::hasTimebaseChanged()
    {
        return timebaseChanged.exchange(false, std::memory_order_acquire);
    }

    Server::TimestampListener::Timestamp Server::TimestampListener::getTimestamp() const noexcept
    {
        Timestamp ts;
        uint32_t before, after;

        do {
            before = sequence.load(std::memory_order_acquire);
            ts.ptpTimeNs = publishedPtpTimeNs.load(std::memory_order_relaxed);
            ts.receiveTimeNs = publishedReceiveTimeNs.load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            after = sequence.load(std::memory_order_relaxed);
        } while (before != after || (before & 1) != 0);

        return ts;
    }

    void Server::TimestampListener::accept(const int64_t ptpTimeNs, const int64_t receiveTimeNs, const bool isNewTimebase)
    {
        current = {ptpTimeNs, receiveTimeNs};
        candidateCount = 0;

        const auto seq{sequence.load(std::memory_order_relaxed)};
        sequence.store(seq + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        publishedPtpTimeNs.store(ptpTimeNs, std::memory_order_relaxed);
        publishedReceiveTimeNs.store(receiveTimeNs, std::memory_order_relaxed);
        sequence.store(seq + 2, std::memory_order_release);

        if (isNewTimebase) {
            timebaseChanged.store(true, std::memory_order_release);
        }
        newTimestampAvailable.store(true, std::memory_order_release);
    }

    void Server::TimestampListener::handlePacket()
    {
        // Check for Follow_Up message (0x08)
        if (numBytesRead < Constants::PTPFollowUpMinSize ||
            (buffer[0] & 0x0f) != Constants::PTPFollowUpMessageType) {
            return;
        }

        const auto receiveTimeNs{getSteadyTimeNs()};
        int64_t seconds{0}, nanoseconds{0};

        // Extract seconds (6 bytes)
        for (int i = 0; i < 6; i++) {
            seconds = seconds << 8 | buffer[34 + i];
        }

        // Extract nanoseconds (4 bytes)
        for (int i = 0; i < 4; i++) {
            nanoseconds = nanoseconds << 8 | buffer[40 + i];
        }

        if (seconds >= Constants::PTPMaxSeconds || nanoseconds >= Constants::NSPS) {
            std::cerr << "Ignoring PTP Follow_Up from " << senderIP << ": invalid timestamp " <<
                    seconds << " s, " << nanoseconds << " ns." << std::endl;
            return;
        }

        const auto ptpTimeNs{seconds * Constants::NSPS + nanoseconds};

        // A Follow_Up should agree with the previous one plus the time that has
        // elapsed locally since it arrived.
        const auto predict{
            [receiveTimeNs](const Timestamp &from) { return from.ptpTimeNs + (receiveTimeNs - from.receiveTimeNs); }
        };
        const auto hasTimebase{current.receiveTimeNs != 0};
        const auto errorNs{hasTimebase ? ptpTimeNs - predict(current) : 0};

        if (hasTimebase && std::abs(errorNs) <= Constants::PTPTimestampToleranceNs) {
            accept(ptpTimeNs, receiveTimeNs, false);
            return;
        }

        // Either a bad timestamp (e.g. a switch relaying garbage, or one that
        // sat in the socket while this thread was held up), or the master's
        // time has genuinely changed. Accept a (new) time base once enough
        // consecutive timestamps agree with one another; two for the first.
        if (candidateCount > 0 && std::abs(ptpTimeNs - predict(candidate)) <= Constants::PTPTimestampToleranceNs) {
            ++candidateCount;
        } else {
            candidateCount = 1;
        }
        candidate = {ptpTimeNs, receiveTimeNs};

        if (candidateCount >= (hasTimebase ? Constants::PTPNewTimebaseCount : 2)) {
            if (hasTimebase) {
                std::cout << "PTP time from " << senderIP << " changed by " << static_cast<double>(errorNs) / Constants::NSPS <<
                        " s; accepting the new time base." << std::endl;
            }
            accept(ptpTimeNs, receiveTimeNs, true);
            return;
        }

        if (hasTimebase) {
            std::cerr << "Ignoring PTP Follow_Up from " << senderIP << ": " << static_cast<double>(errorNs) / Constants::NSPS <<
                    " s away from the expected time." << std::endl;
        }
    }

    //==============================================================================

    Server::ClientListener::ClientListener(
        const Utils::ListenerThreadSocketParams &p,
        ClientList &clients,
        ModuleList &modules
    ) : AnnouncementListenerThread(p),
        clients(clients),
        modules(modules)
    {
    }

    void Server::ClientListener::handlePacket()
    {
        // Firmware that predates numSources/numSpeakers sends a shorter packet;
        // copy only what arrived, so the missing fields stay 0 ("not reported")
        // rather than holding stale bytes from a previous packet.
        if (numBytesRead < static_cast<int>(LegacyClientAnnouncePacketSize)) return;

        ClientAnnouncePacket packet{};
        std::memcpy(&packet, buffer, std::min(sizeof(packet), static_cast<size_t>(numBytesRead)));

        clients.handlePacket(senderIP, packet);
        modules.handlePacket(senderIP, packet);
    }

    //==============================================================================

    Server::AuthorityListener::AuthorityListener(
        const Utils::ListenerThreadSocketParams &p,
        AuthorityInfo &authority
    ) : AnnouncementListenerThread(p),
        authority(authority)
    {
    }

    void Server::AuthorityListener::handlePacket()
    {
        authority.handlePacket(senderIP, reinterpret_cast<AuthorityAnnouncePacket *>(buffer));
    }

    //==========================================================================

    Server::RebootSender::RebootSender(
        const Utils::SenderThreadSocketParams &p,
        ClientList &clients
    ): SenderThread(p),
       clients(clients)
    {
    }

    void Server::RebootSender::runImpl()
    {
        while (!threadShouldExit()) {
            if (clients.getShouldReboot()) {
                clients.setShouldReboot(false);
                socket.write(ip, remotePort, nullptr, 0);
            }

            // Wait 1 second, but check for thread exit every 100ms
            for (int i = 0; i < 10 && !threadShouldExit(); ++i)
                wait(100);
        }
    }

    //==========================================================================

    Server::SwitchInspector::SwitchInspector(
        const Utils::ThreadParams &p,
        SwitchList &switches
    ) : AnanasThread(p),
        switches(switches)
    {
    }

    void Server::SwitchInspector::runImpl()
    {
        std::cout << getThreadName() <<  " inspecting..." << std::endl << std::flush;

        while (!threadShouldExit()) {
            auto switchesVar{switches.toVar()};

            if (auto *obj = switchesVar.getDynamicObject()) {
                for (const auto &prop: obj->getProperties()) {
                    if (const auto *s = prop.value.getDynamicObject()) {
                        auto ip{s->getProperty(Utils::Identifiers::SwitchIpPropertyID).toString()};
                        auto username{s->getProperty(Utils::Identifiers::SwitchUsernamePropertyID).toString()};
                        auto password{s->getProperty(Utils::Identifiers::SwitchPasswordPropertyID).toString()};
                        const bool shouldResetPtp{s->getProperty(Utils::Identifiers::SwitchShouldResetPtpPropertyID)};

                        // Skip incomplete entries, e.g. a newly added row.
                        if (ip.isEmpty() || username.isEmpty() || password.isEmpty()) continue;

                        juce::var jsonData{new juce::DynamicObject};
                        juce::var response;

                        if (shouldResetPtp) {
                            jsonData.getDynamicObject()->setProperty("numbers", "0");
                            response = curlRequest(ip, username, password, Constants::SwitchDisablePtpPath, juce::JSON::toString(jsonData));
                            switches.handleResponse(prop.name, response);
                            response = curlRequest(ip, username, password, Constants::SwitchEnablePtpPath, juce::JSON::toString(jsonData));
                            switches.handleResponse(prop.name, response);
                        } else {
                            jsonData.getDynamicObject()->setProperty("numbers", "0");
                            jsonData.getDynamicObject()->setProperty("once", "");
                            response = curlRequest(ip, username, password, Constants::SwitchMonitorPtpPath, juce::JSON::toString(jsonData));
                            switches.handleResponse(prop.name, response);
                        }
                    }
                }
            }

            // Wait 1 second, but check for thread exit every 100ms
            for (int i = 0; i < 10 && !threadShouldExit(); ++i)
                wait(100);
        }

        std::cout << getThreadName() << " stopping." << std::endl;
    }

    bool Server::SwitchInspector::connect()
    {
        sendChangeMessage();
        return true;
    }

    juce::var Server::SwitchInspector::curlRequest(const juce::String &ip,
                                                   const juce::String &username,
                                                   const juce::String &password,
                                                   const juce::StringRef &path,
                                                   const juce::String &postData)
    {
        const juce::URL url("http://" + ip + path);

        juce::StringArray args;
        args.add("curl");
        args.add("-s"); // Silent, no stats
        args.add("-m" + juce::String{Constants::SwitchInspectorRequestTimeoutS});
        args.add("-k"); // Insecure (no TLS)
        args.add("-u"); // Specify username and password
        args.add(username + ":" + password);
        args.add(url.toString(false));
        args.add("-H");
        args.add("Content-Type: application/json");
        args.add("-d");
        args.add(postData);

        if (curl.start(args)) {
            const auto response{curl.readAllProcessOutput()};
            return juce::JSON::parse(response);
        }
        return juce::var{};
    }
}
