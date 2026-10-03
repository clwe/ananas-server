#include "ClientInfo.h"
#include <AnanasUtils.h>
#include "ServerUtils.h"

namespace ananas
{
    namespace
    {
        // A count of 0 means the firmware doesn't report it.
        int getNumSources(const ClientAnnouncePacket &packet)
        {
            return packet.numSources > 0 ? packet.numSources : Utils::Constants::LegacyNumSources;
        }

        int getNumSpeakers(const ClientAnnouncePacket &packet)
        {
            return packet.numSpeakers > 0 ? packet.numSpeakers : Utils::Constants::LegacyNumSpeakers;
        }
    }

    void ClientInfo::update(const ClientAnnouncePacket &packet)
    {
        lastReceiveTime = juce::Time::getMillisecondCounter();
        info = packet;
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

    void ModuleInfo::update(const ClientAnnouncePacket &packet)
    {
        lastReceiveTime = juce::Time::getMillisecondCounter();
        firmwareType = packet.firmwareType;
        numSources = getNumSources(packet);
        numSpeakers = getNumSpeakers(packet);
        reportedSecondarySource0 = {packet.secondarySource0x, packet.secondarySource0y};
        reportedSecondarySource1 = {packet.secondarySource1x, packet.secondarySource1y};
    }

    juce::ValueTree ModuleInfo::toValueTree() const
    {
        juce::ValueTree tree{Utils::Identifiers::ModuleTreeType};
        tree.setProperty(Utils::Identifiers::ModuleSlotPropertyID, slot, nullptr);
        tree.setProperty(Utils::Identifiers::ModuleFirmwareTypePropertyID, static_cast<int>(firmwareType), nullptr);
        tree.setProperty(Utils::Identifiers::ModuleNumSourcesPropertyID, numSources, nullptr);
        tree.setProperty(Utils::Identifiers::ModuleNumSpeakersPropertyID, numSpeakers, nullptr);
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

    ModuleInfo ModuleInfo::fromValueTree(const juce::ValueTree &tree)
    {
        // Projects saved by older versions have no slot; those modules start
        // unassigned.
        ModuleInfo info;
        info.slot = tree.getProperty(Utils::Identifiers::ModuleSlotPropertyID, -1);
        info.firmwareType = static_cast<Utils::FirmwareType>(static_cast<int>(
            tree.getProperty(Utils::Identifiers::ModuleFirmwareTypePropertyID, static_cast<int>(Utils::FirmwareType::wfsModule))));
        info.numSources = tree.getProperty(Utils::Identifiers::ModuleNumSourcesPropertyID, Utils::Constants::LegacyNumSources);
        info.numSpeakers = tree.getProperty(Utils::Identifiers::ModuleNumSpeakersPropertyID, Utils::Constants::LegacyNumSpeakers);
        return info;
    }

    //==========================================================================

    ClientList::ClientList()
    {
        startTimer(Server::Constants::ClientConnectednessCheckIntervalMs);
    }

    void ClientList::handlePacket(const juce::String &clientIP, const ClientAnnouncePacket &packet)
    {
        {
            const juce::ScopedLock sl{lock};

            auto iter{clients.find(clientIP)};
            if (iter == clients.end()) {
                ClientInfo c{};
                iter = clients.insert(std::make_pair(clientIP, c)).first;
                std::cout << "Client " << iter->first << " connected." << std::endl;
            }
            iter->second.update(packet);
        }

        sendChangeMessage();
    }

    void ClientList::timerCallback()
    {
        checkConnectivity();
    }

    juce::var ClientList::toVar() const
    {
        const juce::ScopedLock sl{lock};
        const auto object{new juce::DynamicObject()};

        for (const auto &[ip, clientInfo]: clients) {
            auto *client{new juce::DynamicObject()};
            const auto packet{clientInfo.getInfo()};
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
                secondarySource1y,
                numSources,
                numSpeakers
            ]{packet};
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
            if (firmwareType == Utils::FirmwareType::wfsModule || firmwareType == Utils::FirmwareType::ambisonicsModule) {
                // Mark assumed (legacy) counts, i.e. those not reported by the firmware.
                client->setProperty(Utils::Identifiers::ClientSourcesSpeakersPropertyID,
                                    juce::String{getNumSources(packet)} + (numSources > 0 ? "" : "*") + " / " +
                                    juce::String{getNumSpeakers(packet)} + (numSpeakers > 0 ? "" : "*"));
            }
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
        const juce::ScopedLock sl{lock};
        return clients.size();
    }

    juce::ValueTree ClientList::toValueTree()
    {
        const juce::ScopedLock sl{lock};
        juce::ValueTree tree(Utils::Identifiers::ConnectedClientsParamID);

        for (const auto &[ip, _]: clients) {
            juce::ValueTree subTree(Utils::Identifiers::ClientTreeType);
            subTree.setProperty("ip", ip, nullptr);
            tree.addChild(subTree, -1, nullptr);
        }

        return tree;
    }

    void ClientList::checkConnectivity()
    {
        bool changed{false};
        {
            const juce::ScopedLock sl{lock};

            for (auto it{clients.begin()}; it != clients.end();) {
                if (!it->second.isConnected()) {
                    std::cout << "Client " << it->first << " disconnected." << std::endl;
                    it = clients.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
        }

        if (changed) sendChangeMessage();
    }

    //==========================================================================

    ModuleList::ModuleList()
    {
        startTimer(Server::Constants::ClientConnectednessCheckIntervalMs);
    }

    void ModuleList::handlePacket(const juce::String &moduleIP, const ClientAnnouncePacket &packet)
    {
        bool changed{false};
        {
            const juce::ScopedLock sl{lock};

            auto iter{modules.find(moduleIP)};
            if (iter == modules.end()) {
                ModuleInfo m{};
                iter = modules.insert(std::make_pair(moduleIP, m)).first;
                std::cout << "Module " << moduleIP << " available." << std::endl;
            }

            auto &module{iter->second};
            const auto previousNumSources{module.numSources}, previousNumSpeakers{module.numSpeakers};
            const auto previousFirmwareType{module.firmwareType};
            module.update(packet);

            if (module.justConnected()) {
                std::cout << "Module " << iter->first << " just connected." << std::endl;
                changed = true;
            }

            if (module.firmwareType != previousFirmwareType) {
                std::cout << "Module " << iter->first << " runs " << Utils::FirmwareTypeToString(module.firmwareType) << " firmware." << std::endl;
                changed = true;
            }

            if (module.numSources != previousNumSources || module.numSpeakers != previousNumSpeakers) {
                std::cout << "Module " << iter->first << " renders " << module.numSources <<
                        " sources on " << module.numSpeakers << " speakers." << std::endl;
                changed = true;
            }
        }

        if (changed) sendChangeMessage();
    }

    void ModuleList::timerCallback()
    {
        checkConnectivity();
    }

    void ModuleList::assignSlot(const int slot, const juce::String &moduleIP)
    {
        bool changed{false};
        {
            const juce::ScopedLock sl{lock};

            for (auto &[ip, m]: modules) {
                const auto newSlot{ip == moduleIP ? slot : (m.slot == slot ? -1 : m.slot)};
                if (newSlot != m.slot) {
                    m.slot = newSlot;
                    changed = true;
                }
            }
        }

        if (changed) sendChangeMessage();
    }

    std::vector<ModuleList::Entry> ModuleList::getEntries() const
    {
        const juce::ScopedLock sl{lock};
        std::vector<Entry> entries;
        entries.reserve(modules.size());

        for (const auto &[ip, m]: modules) {
            entries.push_back({ip, m, m.isConnected()});
        }

        return entries;
    }

    juce::var ModuleList::toVar() const
    {
        const juce::ScopedLock sl{lock};
        const auto object{new juce::DynamicObject()};

        for (const auto &[ip, m]: modules) {
            auto *module{new juce::DynamicObject()};
            module->setProperty(Utils::Identifiers::ModuleSlotPropertyID, m.slot);
            module->setProperty(Utils::Identifiers::ModuleFirmwareTypePropertyID, static_cast<int>(m.firmwareType));
            module->setProperty(Utils::Identifiers::ModuleNumSourcesPropertyID, m.numSources);
            module->setProperty(Utils::Identifiers::ModuleNumSpeakersPropertyID, m.numSpeakers);
            module->setProperty(Utils::Identifiers::ModuleIsConnectedPropertyID, m.isConnected());
            object->setProperty(ip, module);
        }

        return object;
    }

    juce::ValueTree ModuleList::toValueTree() const
    {
        const juce::ScopedLock sl{lock};
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
        {
            const juce::ScopedLock sl{lock};

            modules.clear();

            for (int i{0}; i < tree.getNumChildren(); ++i) {
                auto moduleTree{tree.getChild(i)};
                if (!moduleTree.hasType(Utils::Identifiers::ModuleTreeType)) continue;
                const auto ip{moduleTree.getProperty("ip").toString()};
                if (ip.isEmpty()) continue;
                modules[ip] = ModuleInfo::fromValueTree(moduleTree);
            }
        }

        sendChangeMessage();
    }

    void ModuleList::checkConnectivity()
    {
        bool changed{false};
        {
            const juce::ScopedLock sl{lock};

            for (auto &[ip, m]: modules) {
                if (m.justDisconnected()) {
                    std::cout << "Module " << ip << " just disconnected." << std::endl;
                    changed = true;
                }
            }
        }

        if (changed) sendChangeMessage();
    }
}
