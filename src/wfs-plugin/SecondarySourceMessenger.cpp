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

    void SecondarySourceMessenger::valueTreePropertyChanged(
        juce::ValueTree &treeWhosePropertyHasChanged,
        const juce::Identifier &property
    )
    {
        if (property == ananas::Utils::Identifiers::ModulesParamID) {
            if (auto *obj = treeWhosePropertyHasChanged[property].getDynamicObject()) {
                for (const auto &prop: obj->getProperties()) {
                    auto module{obj->getProperty(prop.name)};
                    auto hasChanged{module.getProperty(ananas::Utils::Identifiers::ModulePositionHasChangedPropertyID, false)};
                    if (hasChanged) {
                        const auto ss0x{module.getProperty(ananas::Utils::Identifiers::ModuleSecondarySource0xPropertyID, 0.f)};
                        const auto ss0y{module.getProperty(ananas::Utils::Identifiers::ModuleSecondarySource0yPropertyID, 0.f)};
                        const auto ss1x{module.getProperty(ananas::Utils::Identifiers::ModuleSecondarySource1xPropertyID, 0.f)};
                        const auto ss1y{module.getProperty(ananas::Utils::Identifiers::ModuleSecondarySource1yPropertyID, 0.f)};

                        juce::OSCBundle bundle;
                        // DBG("Sending OSC: " << Params::getSecondarySourcePositionParamID(0, SourcePositionAxis::X) <<
                        //     " " << juce::String{ss0x} << " to " << prop.name.toString());
                        // DBG("Sending OSC: " << Params::getSecondarySourcePositionParamID(0, SourcePositionAxis::Y) <<
                        //     " " << juce::String{ss0y} << " to " << prop.name.toString());
                        // DBG("Sending OSC: " << Params::getSecondarySourcePositionParamID(1, SourcePositionAxis::X) <<
                        //     " " << juce::String{ss1x} << " to " << prop.name.toString());
                        // DBG("Sending OSC: " << Params::getSecondarySourcePositionParamID(1, SourcePositionAxis::Y) <<
                        //     " " << juce::String{ss1y} << " to " << prop.name.toString());
                        bundle.addElement(juce::OSCMessage{Params::getSecondarySourcePositionParamID(0, SourcePositionAxis::X), static_cast<float>(ss0x)});
                        bundle.addElement(juce::OSCMessage{Params::getSecondarySourcePositionParamID(0, SourcePositionAxis::Y), static_cast<float>(ss0y)});
                        bundle.addElement(juce::OSCMessage{Params::getSecondarySourcePositionParamID(1, SourcePositionAxis::X), static_cast<float>(ss1x)});
                        bundle.addElement(juce::OSCMessage{Params::getSecondarySourcePositionParamID(1, SourcePositionAxis::Y), static_cast<float>(ss1y)});

                        disconnect();
                        if (!connectToSocket(socket, prop.name.toString(), Sockets::SecondarySourceMessengerRemotePort)) {
                            std::cerr << "Failed to connect to socket using address " << prop.name.toString() <<
                                    ":" << Sockets::SecondarySourceMessengerRemotePort << std::endl;
                        }
                        send(bundle);

                        if (auto *moduleObj{module.getDynamicObject()}) {
                            moduleObj->setProperty(ananas::Utils::Identifiers::ModulePositionHasChangedPropertyID, false);
                            obj->setProperty(prop.name, module);
                        }
                    }
                }

                treeWhosePropertyHasChanged.setProperty(property, juce::var{obj}, nullptr);
            }
        }
    }
}
