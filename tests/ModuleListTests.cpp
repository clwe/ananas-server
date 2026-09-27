#include <ClientInfo.h>

namespace
{
    ananas::ClientAnnouncePacket makePacket(const ananas::Utils::FirmwareType type, const uint8_t numSources, const uint8_t numSpeakers)
    {
        ananas::ClientAnnouncePacket p{};
        p.firmwareType = type;
        p.numSources = numSources;
        p.numSpeakers = numSpeakers;
        return p;
    }

    ananas::ModuleList::Entry find(const ananas::ModuleList &list, const juce::String &ip)
    {
        for (const auto &e: list.getEntries()) {
            if (e.ip == ip) return e;
        }
        return {};
    }
}

class ModuleListTests final : public juce::UnitTest
{
public:
    ModuleListTests() : UnitTest("ModuleList", "Server") {}

    void runTest() override
    {
        using Type = ananas::Utils::FirmwareType;

        ananas::ModuleList list;
        list.handlePacket("wfs", makePacket(Type::wfsModule, 8, 16));
        list.handlePacket("legacy", makePacket(Type::wfsModule, 0, 0));
        list.handlePacket("ambi", makePacket(Type::ambisonicsModule, 9, 5));

        beginTest("Announced firmware type and counts, with legacy fallback");
        {
            expect(find(list, "ambi").info.firmwareType == Type::ambisonicsModule);
            expectEquals(find(list, "ambi").info.numSources, 9);
            expectEquals(find(list, "ambi").info.numSpeakers, 5);
            expectEquals(find(list, "legacy").info.numSources, ananas::Utils::Constants::LegacyNumSources);
            expectEquals(find(list, "legacy").info.numSpeakers, ananas::Utils::Constants::LegacyNumSpeakers);
            expect(find(list, "wfs").isConnected);
        }

        beginTest("A slot holds one module, whatever its type");
        {
            list.assignSlot(0, "wfs");
            list.assignSlot(1, "ambi");
            expectEquals(find(list, "wfs").info.slot, 0);
            expectEquals(find(list, "ambi").info.slot, 1);
            list.assignSlot(1, "legacy");
            expectEquals(find(list, "legacy").info.slot, 1);
            expectEquals(find(list, "ambi").info.slot, -1);
            list.assignSlot(1, {});
            expectEquals(find(list, "legacy").info.slot, -1);
        }

        beginTest("Saved and restored with the project");
        {
            list.assignSlot(2, "ambi");
            ananas::ModuleList restored;
            restored.fromValueTree(list.toValueTree());
            expectEquals(static_cast<int>(restored.getEntries().size()), 3);
            const auto ambi{find(restored, "ambi")};
            expectEquals(ambi.info.slot, 2);
            expect(ambi.info.firmwareType == Type::ambisonicsModule);
            expectEquals(ambi.info.numSpeakers, 5);
            expect(!ambi.isConnected);
        }

        beginTest("Projects without firmware type or slot restore as unassigned WFS modules");
        {
            juce::ValueTree tree{ananas::Utils::Identifiers::ModulesParamID};
            juce::ValueTree module{"Module"};
            module.setProperty("ip", "old", nullptr);
            module.setProperty("ModuleSecondarySource0x", -1.5, nullptr);
            tree.addChild(module, -1, nullptr);

            ananas::ModuleList old;
            old.fromValueTree(tree);
            const auto e{find(old, "old")};
            expectEquals(e.info.slot, -1);
            expect(e.info.firmwareType == Type::wfsModule);
            expectEquals(e.info.numSpeakers, ananas::Utils::Constants::LegacyNumSpeakers);
        }
    }
};

static ModuleListTests moduleListTests;
