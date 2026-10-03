#include <SwitchInfo.h>
#include <AnanasUtils.h>

class SwitchListTests final : public juce::UnitTest
{
public:
    SwitchListTests() : UnitTest("SwitchList", "Server") {}

    void runTest() override
    {
        ananas::SwitchList list;

        auto *edit{new juce::DynamicObject()};
        edit->setProperty("switch_1", makeSwitch("192.168.10.2", "admin", "secret"));
        edit->setProperty("switch_2", makeSwitch("192.168.10.3", "admin", "other"));
        list.handleEdit(edit);

        const auto saved{list.toValueTree()};
        ananas::SwitchList restored;
        restored.fromValueTree(saved);

        beginTest("Save and restore");
        {
            expectEquals(saved.getNumChildren(), 2);
            expectEquals(saved.getChild(0).getProperty(ananas::Utils::Identifiers::SwitchIpPropertyID).toString(),
             juce::String{"192.168.10.2"});
            expect(restored.toValueTree().isEquivalentTo(saved));
        }

        auto withForeignChild{saved.createCopy()};
        juce::ValueTree foreign{"NotASwitch"};
        foreign.setProperty("identifier", "switch_3", nullptr);
        withForeignChild.addChild(foreign, -1, nullptr);

        ananas::SwitchList filtered;
        filtered.fromValueTree(withForeignChild);

        beginTest("Filter foreign nodes");
        {
            expect(filtered.toValueTree().isEquivalentTo(saved));   // switch_3 must not appear
        }


    }

    juce::var makeSwitch(const juce::String &ip, const juce::String &user, const juce::String &password)
    {
        auto *s{new juce::DynamicObject()};
        s->setProperty(ananas::Utils::Identifiers::SwitchIpPropertyID, ip);
        s->setProperty(ananas::Utils::Identifiers::SwitchUsernamePropertyID, user);
        s->setProperty(ananas::Utils::Identifiers::SwitchPasswordPropertyID, password);
        return s;
    }
};

static SwitchListTests switchListTests;
