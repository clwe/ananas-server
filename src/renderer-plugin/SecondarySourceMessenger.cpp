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

        if (!sendBundle(moduleIP, bundle)) return false;

        std::cout << "Sent " << positions.size() << " speaker positions to " << moduleIP << std::endl;
        return true;
    }

    bool SecondarySourceMessenger::sendGeometry(const juce::String &moduleIP, const juce::uint32 setId,
                                                const std::vector<OutputGeometry> &outputs)
    {
        jassert(setId != 0); // 0 means "nothing received" to the module

        // About 870 bytes for 16 outputs: one datagram.
        juce::OSCBundle bundle;

        // OSC has no unsigned int; the module reads the same 32 bits back
        // as uint32.
        juce::OSCMessage id{"/ss/id"};
        id.addInt32(static_cast<juce::int32>(setId));
        bundle.addElement(id);

        for (size_t j{0}; j < outputs.size(); ++j) {
            const auto &o{outputs[j]};
            juce::OSCMessage output{"/ss/" + juce::String{j}};
            output.addFloat32(o.position.x);
            output.addFloat32(o.position.y);
            output.addFloat32(o.z);
            output.addFloat32(o.facing.x);
            output.addFloat32(o.facing.y);
            output.addFloat32(o.facing.z);
            output.addFloat32(o.isUsed ? 1.f : 0.f);
            bundle.addElement(output);
        }

        if (!sendBundle(moduleIP, bundle)) return false;

        std::cout << "Sent " << outputs.size() << " speaker positions (set " << setId << ") to " << moduleIP << std::endl;
        return true;
    }

    bool SecondarySourceMessenger::sendBundle(const juce::String &moduleIP, const juce::OSCBundle &bundle)
    {
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

        return true;
    }
}
