#include "ClientInfo.h"
#include <AnanasUtils.h>
#include "ServerUtils.h"

namespace ananas
{
    void ClientInfo::update(const ClientAnnouncePacket *packet)
    {
        lastReceiveTime = juce::Time::getMillisecondCounter();
        info = *packet;
    }

    bool ClientInfo::isConnected() const
    {
        return juce::Time::getMillisecondCounter() - lastReceiveTime < Server::Sockets::ClientListenerSocketParams.disconnectionThresholdMs;
    }

    ClientAnnouncePacket ClientInfo::getInfo() const
    {
        return info;
    }

    //==========================================================================

    void ModuleInfo::update()
    {
        lastReceiveTime = juce::Time::getMillisecondCounter();
    }

    juce::ValueTree ModuleInfo::toValueTree() const
    {
        juce::ValueTree tree{"Module"};
        tree.setProperty(Utils::Identifiers::ModuleSecondarySource0xPropertyID, secondarySource0Position.first, nullptr);
        tree.setProperty(Utils::Identifiers::ModuleSecondarySource0yPropertyID, secondarySource0Position.second, nullptr);
        tree.setProperty(Utils::Identifiers::ModuleSecondarySource1xPropertyID, secondarySource1Position.first, nullptr);
        tree.setProperty(Utils::Identifiers::ModuleSecondarySource1yPropertyID, secondarySource1Position.second, nullptr);
        return tree;
    }

    bool ModuleInfo::isConnected() const
    {
        return juce::Time::getMillisecondCounter() - lastReceiveTime < Server::Sockets::ClientListenerSocketParams.disconnectionThresholdMs;
    }

    bool ModuleInfo::justDisconnected()
    {
        const auto didJustDisconnect{!isConnected() && wasConnected};

        if (didJustDisconnect) {
            wasConnected = false;
        }

        return didJustDisconnect;
    }

    bool ModuleInfo::justConnected()
    {
        const auto didJustConnect{isConnected() && !wasConnected};

        if (didJustConnect) {
            wasConnected = true;
        }

        return didJustConnect;
    }

    std::pair<float, float> ModuleInfo::getSecondarySource0Position() const
    {
        return secondarySource0Position;
    }

    std::pair<float, float> ModuleInfo::getSecondarySource1Position() const
    {
        return secondarySource1Position;
    }

    ModuleInfo ModuleInfo::fromValueTree(const juce::ValueTree &tree)
    {
        ModuleInfo info;
        const auto &prop0{tree.getProperty(Utils::Identifiers::ModuleSecondarySource0xPropertyID)};
        if (prop0.isDouble()) {
            const float val{prop0};
            info.secondarySource0Position.first = val;
        }
        const auto &prop1{tree.getProperty(Utils::Identifiers::ModuleSecondarySource0yPropertyID)};
        if (prop1.isDouble()) {
            const float val{prop1};
            info.secondarySource0Position.second = val;
        }
        const auto &prop2{tree.getProperty(Utils::Identifiers::ModuleSecondarySource0xPropertyID)};
        if (prop2.isDouble()) {
            const float val{prop2};
            info.secondarySource1Position.first = val;
        }
        const auto &prop3{tree.getProperty(Utils::Identifiers::ModuleSecondarySource0yPropertyID)};
        if (prop3.isDouble()) {
            const float val{prop3};
            info.secondarySource1Position.second = val;
        }
        return info;
    }

    //==========================================================================

    ClientList::ClientList()
    {
        startTimer(Server::Constants::ClientConnectednessCheckIntervalMs);
    }

    void ClientList::handlePacket(const juce::String &clientIP, const ClientAnnouncePacket *packet)
    {
        auto iter{clients.find(clientIP)};
        if (iter == clients.end()) {
            ClientInfo c{};
            iter = clients.insert(std::make_pair(clientIP, c)).first;
            std::cout << "Client " << iter->first << " connected." << std::endl;
        }
        iter->second.update(packet);

        sendChangeMessage();
    }

    void ClientList::timerCallback()
    {
        checkConnectivity();
    }

    juce::var ClientList::toVar() const
    {
        const auto object{new juce::DynamicObject()};

        for (const auto &[ip, clientInfo]: clients) {
            auto *client{new juce::DynamicObject()};
            const auto &[
                serial,
                firmwareType,
                firmwareVersion,
                samplingRate,
                percentCPU,
                presentationOffsetFrame,
                presentationOffsetNs,
                audioPTPOffsetNs,
                bufferFillPercent,
                ptpLock,
                secondarySource0x,
                secondarySource0y,
                secondarySource1x,
                secondarySource1y
            ]{clientInfo.getInfo()};
            client->setProperty(Utils::Identifiers::ClientSerialNumberPropertyID, static_cast<int>(serial));
            client->setProperty(Utils::Identifiers::ClientFirmwareTypeVersionPropertyID,
                                Utils::FirmwareTypeToString(firmwareType) + " v" + Utils::VersionNumberToString(firmwareVersion));
            client->setProperty(Utils::Identifiers::ClientPTPLockPropertyID, ptpLock);
            client->setProperty(Utils::Identifiers::ClientPresentationTimeOffsetNsPropertyID, presentationOffsetNs);
            client->setProperty(Utils::Identifiers::ClientPresentationTimeOffsetFramePropertyID, presentationOffsetFrame);
            client->setProperty(Utils::Identifiers::ClientAudioPTPOffsetPropertyID, audioPTPOffsetNs);
            client->setProperty(Utils::Identifiers::ClientBufferFillPercentPropertyID, bufferFillPercent);
            client->setProperty(Utils::Identifiers::ClientSamplingRatePropertyID, samplingRate);
            client->setProperty(Utils::Identifiers::ClientPercentCPUPropertyID, percentCPU);
            client->setProperty(Utils::Identifiers::ClientSecondarySourceCoordinatesPropertyID,
                                "(" + juce::String{secondarySource0x} + ", " + juce::String{secondarySource0y} +
                                "), (" + juce::String{secondarySource1x} + ", " + juce::String{secondarySource1y} + ")");
            object->setProperty(ip, client);
        }

        return object;
    }

    bool ClientList::getShouldReboot() const
    {
        return shouldReboot;
    }

    void ClientList::setShouldReboot(const bool should)
    {
        shouldReboot = should;
    }

    uint ClientList::getCount() const
    {
        return clients.size();
    }

    juce::ValueTree ClientList::toValueTree()
    {
        juce::ValueTree tree(Utils::Identifiers::ConnectedClientsParamID);

        for (const auto &[ip, _]: clients) {
            juce::ValueTree subTree("Client");
            subTree.setProperty("ip", ip, nullptr);
            tree.addChild(subTree, -1, nullptr);
        }

        return tree;
    }

    void ClientList::checkConnectivity()
    {
        std::vector<juce::String> toErase;
        for (const auto &[ip, c]: clients) {
            if (!c.isConnected()) {
                std::cout << "Client " << ip << " disconnected." << std::endl;
                toErase.push_back(ip);
            }
        }
        for (const auto &ip: toErase) {
            clients.erase(ip);
            sendChangeMessage();
        }
    }

    //==========================================================================

    ModuleList::ModuleList()
    {
        startTimer(Server::Constants::ClientConnectednessCheckIntervalMs);
    }

    void ModuleList::handlePacket(const juce::String &moduleIP)
    {
        auto iter{modules.find(moduleIP)};
        if (iter == modules.end()) {
            ModuleInfo m{};
            iter = modules.insert(std::make_pair(moduleIP, m)).first;
            std::cout << "Module " << moduleIP << " available." << std::endl;
        }
        iter->second.update();
        if (iter->second.justConnected()) {
            std::cout << "Module " << iter->first << " just connected." << std::endl;
            sendChangeMessage();
        }
    }

    void ModuleList::timerCallback()
    {
        checkConnectivity();
    }

    juce::var ModuleList::toVar() const
    {
        const auto object{new juce::DynamicObject()};

        for (const auto &[ip, m]: modules) {
            auto *module{new juce::DynamicObject()};
            module->setProperty(Utils::Identifiers::ModuleSecondarySource0xPropertyID, static_cast<int>(m.getSecondarySource0Position().first));
            module->setProperty(Utils::Identifiers::ModuleSecondarySource0yPropertyID, static_cast<int>(m.getSecondarySource0Position().second));
            module->setProperty(Utils::Identifiers::ModuleSecondarySource1xPropertyID, static_cast<int>(m.getSecondarySource1Position().first));
            module->setProperty(Utils::Identifiers::ModuleSecondarySource1yPropertyID, static_cast<int>(m.getSecondarySource1Position().second));
            module->setProperty(Utils::Identifiers::ModuleIsConnectedPropertyID, m.isConnected());
            object->setProperty(ip, module);
        }

        return object;
    }

    juce::ValueTree ModuleList::toValueTree() const
    {
        juce::ValueTree tree(Utils::Identifiers::ModulesParamID);

        for (const auto &[ip, m]: modules) {
            auto moduleTree{m.toValueTree()};
            moduleTree.setProperty("ip", ip, nullptr);
            tree.addChild(moduleTree, -1, nullptr);
        }

        return tree;
    }

    void ModuleList::fromValueTree(const juce::ValueTree &tree)
    {
        modules.clear();

        for (int i{0}; i < tree.getNumChildren(); ++i) {
            auto moduleTree{tree.getChild(i)};
            juce::Identifier ip{moduleTree.getProperty("ip")};
            modules[ip.toString()] = ModuleInfo::fromValueTree(moduleTree);
        }
    }

    void ModuleList::checkConnectivity()
    {
        for (auto it{modules.begin()}; it != modules.end(); ++it) {
            if (it->second.justDisconnected()) {
                std::cout << "Module " << it->first << " just disconnected." << std::endl;
                sendChangeMessage();
            }
        }
    }
}
