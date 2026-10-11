#include <LinearArray.h>
#include <WFSUtils.h>

namespace
{
    // Counts the change messages a setup sends.
    struct ChangeCounter final : juce::ChangeListener
    {
        explicit ChangeCounter(ananas::WFS::LinearArraySetup &s) : setup(s) { setup.addChangeListener(this); }

        ~ChangeCounter() override { setup.removeChangeListener(this); }

        void changeListenerCallback(juce::ChangeBroadcaster *) override { ++count; }

        // Change messages are asynchronous; deliver any pending one now.
        // Several changes before a delivery count as one.
        int deliver()
        {
            setup.dispatchPendingMessages();
            return count;
        }

        ananas::WFS::LinearArraySetup &setup;
        int count = 0;
    };

    bool approximatelyEqual(const float a, const float b) { return std::abs(a - b) < 1e-5f; }
}

class LinearArraySetupTests final : public juce::UnitTest
{
public:
    LinearArraySetupTests() : UnitTest("LinearArraySetup", "Renderer") {}

    void runTest() override
    {
        using ananas::WFS::LinearArraySetup;
        using ananas::WFS::Constants;
        const auto maxSlots = static_cast<int>(Constants::MaxNumModules);

        beginTest("Defaults: one empty entry per slot");
        {
            const LinearArraySetup setup;
            const auto &s = setup.getSettings();

            expectEquals(s.numSlots, Constants::DefaultNumSlots);
            expect(approximatelyEqual(s.speakerSpacing, Constants::DefaultSpeakerSpacingMetres));
            expectEquals(static_cast<int>(s.slotModuleIps.size()), s.numSlots);
            for (const auto &ip: s.slotModuleIps) {
                expect(ip.isEmpty());
            }
        }

        beginTest("Number of slots and spacing are clamped");
        {
            LinearArraySetup setup;

            setup.setNumSlots(0);
            expectEquals(setup.getSettings().numSlots, Constants::MinNumSlots);
            setup.setNumSlots(maxSlots + 5);
            expectEquals(setup.getSettings().numSlots, maxSlots);
            expectEquals(static_cast<int>(setup.getSettings().slotModuleIps.size()), maxSlots);

            setup.setSpeakerSpacing(0.f);
            expect(approximatelyEqual(setup.getSettings().speakerSpacing, Constants::MinSpeakerSpacingMetres));
            setup.setSpeakerSpacing(10.f);
            expect(approximatelyEqual(setup.getSettings().speakerSpacing, Constants::MaxSpeakerSpacingMetres));
        }

        beginTest("Assigning a module moves it out of its old slot");
        {
            LinearArraySetup setup;

            setup.assignModule(0, "a");
            setup.assignModule(1, "b");
            expectEquals(setup.getSettings().slotModuleIps[0], juce::String{"a"});
            expectEquals(setup.getSettings().slotModuleIps[1], juce::String{"b"});

            setup.assignModule(3, "a");
            expect(setup.getSettings().slotModuleIps[0].isEmpty());
            expectEquals(setup.getSettings().slotModuleIps[3], juce::String{"a"});

            // Replacing a module just takes its slot; the replaced one is unassigned.
            setup.assignModule(1, "c");
            expectEquals(setup.getSettings().slotModuleIps[1], juce::String{"c"});
            for (const auto &ip: setup.getSettings().slotModuleIps) {
                expect(ip != "b");
            }
        }

        beginTest("An empty IP clears a slot; slots out of range are ignored");
        {
            LinearArraySetup setup;
            setup.assignModule(2, "a");

            setup.assignModule(2, {});
            expect(setup.getSettings().slotModuleIps[2].isEmpty());

            setup.assignModule(-1, "b");
            setup.assignModule(setup.getSettings().numSlots, "b");
            for (const auto &ip: setup.getSettings().slotModuleIps) {
                expect(ip.isEmpty());
            }
        }

        beginTest("Fewer slots drop the modules in the removed slots");
        {
            LinearArraySetup setup;
            setup.assignModule(1, "a");
            setup.assignModule(5, "b");

            setup.setNumSlots(3);
            const auto &s = setup.getSettings();
            expectEquals(static_cast<int>(s.slotModuleIps.size()), 3);
            expectEquals(s.slotModuleIps[1], juce::String{"a"});

            // Growing again gives empty slots; "b" is gone.
            setup.setNumSlots(8);
            expect(setup.getSettings().slotModuleIps[5].isEmpty());
        }

        beginTest("Save and restore");
        {
            LinearArraySetup setup;
            setup.setNumSlots(4);
            setup.setSpeakerSpacing(.15f);
            setup.assignModule(0, "a");
            setup.assignModule(3, "b");

            LinearArraySetup restored;
            restored.fromValueTree(setup.toValueTree());
            const auto &s = restored.getSettings();

            expectEquals(s.numSlots, 4);
            expect(approximatelyEqual(s.speakerSpacing, .15f));
            expect(s.slotModuleIps == std::vector<juce::String>{"a", "", "", "b"});
            expect(restored.toValueTree().isEquivalentTo(setup.toValueTree()));
        }

        beginTest("Restoring applies the same rules");
        {
            juce::ValueTree tree(ananas::WFS::Identifiers::LinearArrayTreeType);
            tree.setProperty(ananas::WFS::Identifiers::LinearArrayNumSlotsPropertyID, 3, nullptr);
            tree.setProperty(ananas::WFS::Identifiers::LinearArraySpeakerSpacingPropertyID, 5.f, nullptr);

            const auto addSlot = [&tree](const int index, const juce::String &ip)
            {
                juce::ValueTree slot(ananas::WFS::Identifiers::LinearArraySlotTreeType);
                slot.setProperty(ananas::WFS::Identifiers::LinearArraySlotIndexPropertyID, index, nullptr);
                slot.setProperty(ananas::WFS::Identifiers::LinearArraySlotModuleIpPropertyID, ip, nullptr);
                tree.addChild(slot, -1, nullptr);
            };
            addSlot(0, "a");
            addSlot(2, "a");    // the same module twice: the leftmost slot wins
            addSlot(7, "b");    // beyond the number of slots
            tree.addChild(juce::ValueTree{"NotASlot"}, -1, nullptr);

            LinearArraySetup setup;
            setup.fromValueTree(tree);
            const auto &s = setup.getSettings();

            expectEquals(s.numSlots, 3);
            expect(approximatelyEqual(s.speakerSpacing, Constants::MaxSpeakerSpacingMetres));
            expect(s.slotModuleIps == std::vector<juce::String>{"a", "", ""});
        }

        beginTest("A tree of another type is ignored");
        {
            LinearArraySetup setup;
            setup.assignModule(0, "a");

            setup.fromValueTree(juce::ValueTree{"Something"});
            expectEquals(setup.getSettings().slotModuleIps[0], juce::String{"a"});
        }

        beginTest("Change messages only for real changes");
        {
            LinearArraySetup setup;
            ChangeCounter counter(setup);

            setup.setNumSlots(Constants::DefaultNumSlots);
            setup.setSpeakerSpacing(Constants::DefaultSpeakerSpacingMetres);
            setup.assignModule(0, {});
            expectEquals(counter.deliver(), 0);

            setup.assignModule(0, "a");
            expectEquals(counter.deliver(), 1);

            setup.assignModule(0, "a");
            expectEquals(counter.deliver(), 1);

            setup.setSpeakerSpacing(.1f);
            expectEquals(counter.deliver(), 2);

            // Clamped to the same value as before: no change.
            setup.setNumSlots(maxSlots);
            counter.deliver();
            const auto before = counter.count;
            setup.setNumSlots(maxSlots + 1);
            expectEquals(counter.deliver(), before);
        }
    }
};

static LinearArraySetupTests linearArraySetupTests;
