#include "VirtualSourceMessenger.h"
#include <AnanasUtils.h>
#include "WFSUtils.h"

namespace ananas::WFS
{
    VirtualSourceMessenger::VirtualSourceMessenger(
        const ananas::Utils::SenderThreadSocketParams &p,
        juce::AudioProcessorValueTreeState &apvts
    )
        : Thread(p.name),
          ip(p.ip),
          localPort(p.localPort),
          remotePort(p.remotePort),
          state(apvts)
    {
        for (uint n{0}; n < Constants::NumSources; ++n) {
            state.addParameterListener(Params::getVirtualSourcePositionParamID(n, SourcePositionAxis::X), this);
            state.addParameterListener(Params::getVirtualSourcePositionParamID(n, SourcePositionAxis::Y), this);
        }
        // Limit the rate of source position updates and their corresponding OSC
        // sends, to thirty per second.
        startTimerHz(30);
    }

    VirtualSourceMessenger::~VirtualSourceMessenger()
    {
        for (uint n{0}; n < Constants::NumSources; ++n) {
            state.removeParameterListener(Params::getVirtualSourcePositionParamID(n, SourcePositionAxis::X), this);
            state.removeParameterListener(Params::getVirtualSourcePositionParamID(n, SourcePositionAxis::Y), this);
        }
    }

    bool VirtualSourceMessenger::connect()
    {
        if (!socket.bindToPort(localPort, ananas::Utils::Strings::LocalInterfaceIP)) {
            std::cerr << "WFS Messenger failed to bind to port: " << std::strerror(errno) << std::endl;
            return false;
        }

        if (!connectToSocket(socket, ip, remotePort)) {
            std::cerr << "WFS Messenger failed to connect to socket: " << std::strerror(errno) << std::endl;
            return false;
        }

        connected = true;
        return true;
    }

    void VirtualSourceMessenger::run()
    {
        while (!connect() && !threadShouldExit()) {
            for (int i = 0; i < 50 && !threadShouldExit(); ++i)
                sleep(100);
        }

        runImpl();
    }

    void VirtualSourceMessenger::parameterChanged(const juce::String &parameterID, float newValue)
    {
        // Map the virtual source co-ordinate onto the source area.
        const auto area{sourceArea.load()};
        if (parameterID.endsWith("x")) {
            newValue = area.toMetres({newValue, 0.f}).x;
        } else {
            newValue = area.toMetres({0.f, newValue}).y;
        }

        // Indicate that a parameter change message should be sent at the next
        // timer callback.
        slots[parameterID].value.store(newValue);
        slots[parameterID].changed.store(true);
    }

    void VirtualSourceMessenger::setSourceArea(const SourceArea &area)
    {
        sourceArea.store(area);
    }

    void VirtualSourceMessenger::timerCallback()
    {
        if (!connected) { return; }

        // Send all changed parameters.
        for (auto &[id, slot]: slots) {
            if (slot.changed.load()) {
                juce::OSCBundle bundle;
                DBG("Sending OSC: " << id << " " << slot.value.load());
                bundle.addElement(juce::OSCMessage{id, slot.value.load()});
                send(bundle);
                slot.changed.store(false);
                // Send one parameter update per timer callback.
                // NB. could be an issue if many parameters are changing
                // simultaneously, e.g. due to automation
                return;
            }
        }
    }

    void VirtualSourceMessenger::runImpl() const
    {
        while (!threadShouldExit()) {
            // for (int i = 0; i < 10 && !threadShouldExit(); ++i)
            // wait(100);
        }
    }
} // ananas::WFS
