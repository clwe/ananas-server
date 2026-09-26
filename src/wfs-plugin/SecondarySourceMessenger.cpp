#include "SecondarySourceMessenger.h"
#include <AnanasUtils.h>

#include "WFSUtils.h"

namespace ananas::WFS
{
    SecondarySourceMessenger::SecondarySourceMessenger()
    {
        if (!socket.bindToPort(Sockets::SecondarySourceMessengerLocalPort, ananas::Utils::Strings::LocalInterfaceIP)) {
            std::cerr << "Secondary Source Messenger failed to bind to port: " << std::strerror(errno) << std::endl;
        }
    }

    bool SecondarySourceMessenger::sendPositions(const juce::String &moduleIP, const std::vector<juce::Point<float>> &positions)
    {
        // For 16 speakers this is about 780 bytes; module firmware from before
        // the receive buffer fix (rxBuffer[128]) crashes on bundles this size.
        juce::OSCBundle bundle;

        for (size_t j{0}; j < positions.size(); ++j) {
            const auto index{static_cast<uint>(j)};
            bundle.addElement(juce::OSCMessage{Params::getSecondarySourcePositionParamID(index, SourcePositionAxis::X), positions[j].x});
            bundle.addElement(juce::OSCMessage{Params::getSecondarySourcePositionParamID(index, SourcePositionAxis::Y), positions[j].y});
        }

        disconnect();
        if (!connectToSocket(socket, moduleIP, Sockets::SecondarySourceMessengerRemotePort)) {
            std::cerr << "Failed to connect to socket using address " << moduleIP <<
                    ":" << Sockets::SecondarySourceMessengerRemotePort << std::endl;
            return false;
        }

        if (!send(bundle)) {
            // E.g. no route to the module; its address must be on the same
            // subnet as the local interface.
            std::cerr << "Failed to send speaker positions to " << moduleIP << ": " << std::strerror(errno) << std::endl;
            return false;
        }

        std::cout << "Sent " << positions.size() << " speaker positions to " << moduleIP << std::endl;
        return true;
    }
}
