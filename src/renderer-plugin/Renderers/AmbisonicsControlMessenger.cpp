#include "AmbisonicsControlMessenger.h"

namespace ananas::Ambisonics
{
    ControlMessenger::ControlMessenger(const Utils::SenderThreadSocketParams &p)
        : ip(p.ip),
          localPort(p.localPort),
          remotePort(p.remotePort)
    {
    }

    bool ControlMessenger::connect()
    {
        if (connected) return true;

        if (socket.getBoundPort() < 0 && !socket.bindToPort(localPort, Utils::Strings::LocalInterfaceIP)) {
            std::cerr << "Ambisonics control messenger failed to bind to port: " << std::strerror(errno) << std::endl;
            return false;
        }

        connected = connectToSocket(socket, ip, remotePort);
        if (!connected) {
            std::cerr << "Ambisonics control messenger failed to connect to " << ip << ":" << remotePort << std::endl;
        }
        return connected;
    }

    void ControlMessenger::send(const juce::Point<float> listener, const float rmax, const bool force)
    {
        const auto listenerChanged{force || listener != lastListener};
        const auto rmaxChanged{rmax >= 0.f && (force || !juce::approximatelyEqual(rmax, lastRmax))};

        if (!(listenerChanged || rmaxChanged) || !connect()) return;

        juce::OSCBundle bundle;
        if (listenerChanged) {
            bundle.addElement(juce::OSCMessage{"/listener/x", listener.x});
            bundle.addElement(juce::OSCMessage{"/listener/y", listener.y});
        }
        if (rmaxChanged) {
            bundle.addElement(juce::OSCMessage{"/ambi/rmax", rmax});
        }

        if (OSCSender::send(bundle)) {
            if (listenerChanged) lastListener = listener;
            if (rmaxChanged) lastRmax = rmax;
        } else {
            std::cerr << "Failed to send Ambisonics control messages: " << std::strerror(errno) << std::endl;
        }
    }
}
