#ifndef CLIENTINFO_H
#define CLIENTINFO_H

#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Packet.h"

namespace ananas
{
    class ClientInfo
    {
    public:
        void update(const ClientAnnouncePacket &packet);

        [[nodiscard]] bool isConnected() const;

        [[nodiscard]] ClientAnnouncePacket getInfo() const;

    private:
        ClientAnnouncePacket info{};
        uint32_t lastReceiveTime{0};
    };

    class ModuleInfo
    {
    public:
        void update(const ClientAnnouncePacket &packet);

        [[nodiscard]] juce::ValueTree toValueTree() const;

        [[nodiscard]] bool isConnected() const;

        [[nodiscard]] bool justDisconnected();

        [[nodiscard]] bool justConnected();

        static ModuleInfo fromValueTree(const juce::ValueTree &tree);

        // Which rendering technique the module's firmware implements.
        Utils::FirmwareType firmwareType{Utils::FirmwareType::wfsModule};
        // Virtual sources (WFS) or Ambisonic channels (Ambisonics) this module
        // renders, and speaker outputs it drives.
        int numSources{Utils::Constants::LegacyNumSources};
        int numSpeakers{Utils::Constants::LegacyNumSpeakers};
        // Positions of ss/0 and ss/1 as echoed in the module's announcements.
        std::pair<float, float> reportedSecondarySource0{0.f, 0.f};
        std::pair<float, float> reportedSecondarySource1{0.f, 0.f};
        // Speaker messages the module understands (see ClientAnnouncePacket),
        // and the last speaker set id it echoed. Not saved: they come from the
        // module, and change with its firmware.
        int speakerProtocol{0};
        juce::uint32 speakerSetId{0};

    private:
        juce::uint32 lastReceiveTime{0};
        bool wasConnected{false};
    };

    class ClientList final : public juce::Timer,
                             public juce::ChangeBroadcaster
    {
    public:
        ClientList();

        void handlePacket(const juce::String &clientIP, const ClientAnnouncePacket &packet);

        void timerCallback() override;

        [[nodiscard]] juce::var toVar() const;

        void requestReboot();

        [[nodiscard]] bool takeRebootRequest();

        [[nodiscard]] uint getCount() const;

        juce::ValueTree toValueTree();

    private:
        void checkConnectivity();

        // Accessed from the message thread and the client listener thread.
        mutable juce::CriticalSection lock;
        std::map<juce::String, ClientInfo> clients;
        std::atomic<bool> shouldReboot{false};
    };

    class ModuleList final : public juce::ChangeBroadcaster,
                             public juce::Timer
    {
    public:
        struct Entry
        {
            juce::String ip;
            ModuleInfo info;
            bool isConnected;
        };

        ModuleList();

        void handlePacket(const juce::String &moduleIP, const ClientAnnouncePacket &packet);

        void timerCallback() override;

        [[nodiscard]] std::vector<Entry> getEntries() const;

        [[nodiscard]] juce::ValueTree toValueTree(bool includeConnectionState = false) const;

        void fromValueTree(const juce::ValueTree &tree);

    private:
        void checkConnectivity();

        // Accessed from the message thread and the client listener thread.
        mutable juce::CriticalSection lock;
        std::map<juce::String, ModuleInfo> modules;
    };
}


#endif //CLIENTINFO_H
