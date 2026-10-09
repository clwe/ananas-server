#include <AnanasUtils.h>

namespace
{
    const juce::Identifier ListType{"List"};
    const juce::Identifier ItemType{"Item"};
    const juce::Identifier Key{"ip"};
    const juce::Identifier Slot{"slot"};
    const juce::Identifier Connected{"connected"};

    juce::ValueTree makeItem(const juce::String &ip, const int slot)
    {
        juce::ValueTree item(ItemType);
        item.setProperty(Key, ip, nullptr);
        item.setProperty(Slot, slot, nullptr);
        return item;
    }

    // A list with one child per (ip, slot) pair, in the given order.
    juce::ValueTree makeList(std::initializer_list<std::pair<const char *, int>> items)
    {
        juce::ValueTree list(ListType);
        for (const auto &[ip, slot]: items) {
            list.addChild(makeItem(ip, slot), -1, nullptr);
        }
        return list;
    }

    // Counts the notifications a tree sends, including those of its children.
    struct NotificationCounter final : juce::ValueTree::Listener
    {
        explicit NotificationCounter(juce::ValueTree &t) : tree(t) { tree.addListener(this); }

        ~NotificationCounter() override { tree.removeListener(this); }

        void valueTreePropertyChanged(juce::ValueTree &, const juce::Identifier &) override { ++propertiesChanged; }

        void valueTreeChildAdded(juce::ValueTree &, juce::ValueTree &) override { ++childrenAdded; }

        void valueTreeChildRemoved(juce::ValueTree &, juce::ValueTree &, int) override { ++childrenRemoved; }

        juce::ValueTree &tree;
        int propertiesChanged = 0;
        int childrenAdded = 0;
        int childrenRemoved = 0;
    };
}

class ValueTreeUtilsTests final : public juce::UnitTest
{
public:
    ValueTreeUtilsTests() : UnitTest("ValueTreeUtils", "Utils") {}

    void runTest() override
    {
        beginTest("syncChildren: no change sends no notifications");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}});
            const auto source = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 0, 0, 0);
        }

        beginTest("syncChildren: a changed property is one notification");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}});
            const auto source = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 5}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 1, 0, 0);
        }

        beginTest("syncChildren: children are updated in place, not replaced");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}});
            const auto source = makeList({{"10.0.0.1", 3}, {"10.0.0.2", 1}});
            const auto firstChild = target.getChild(0);

            ananas::Utils::syncChildren(target, source, Key);

            // A handle held before the sync still refers to the child in the
            // target, and sees the new value.
            expect(firstChild.getParent() == target);
            expectEquals(static_cast<int>(firstChild[Slot]), 3);
        }

        beginTest("syncChildren: a new child is added in the source's order");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.3", 2}});
            const auto source = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}, {"10.0.0.3", 2}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            // isEquivalentTo also compares the order of the children.
            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 0, 1, 0);
        }

        beginTest("syncChildren: an empty target is filled in order");
        {
            juce::ValueTree target(ListType);
            const auto source = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}, {"10.0.0.3", 2}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 0, 3, 0);
        }

        beginTest("syncChildren: a child missing from the source is removed");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}, {"10.0.0.3", 2}});
            const auto source = makeList({{"10.0.0.1", 0}, {"10.0.0.3", 2}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 0, 0, 1);
        }

        beginTest("syncChildren: all children removed");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}});
            const juce::ValueTree source(ListType);
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expectEquals(target.getNumChildren(), 0);
            expectNotifications(counter, 0, 0, 2);
        }

        beginTest("syncChildren: properties missing from the source are removed");
        {
            auto target = makeList({{"10.0.0.1", 0}});
            target.getChild(0).setProperty(Connected, true, nullptr);
            const auto source = makeList({{"10.0.0.1", 0}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expect(!target.getChild(0).hasProperty(Connected));
            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 1, 0, 0);
        }

        beginTest("syncChildren: add, remove and change at once");
        {
            auto target = makeList({{"10.0.0.1", 0}, {"10.0.0.2", 1}, {"10.0.0.3", 2}});
            const auto source = makeList({{"10.0.0.2", 7}, {"10.0.0.3", 2}, {"10.0.0.4", 3}});
            NotificationCounter counter(target);

            ananas::Utils::syncChildren(target, source, Key);

            expect(target.isEquivalentTo(source));
            expectNotifications(counter, 1, 1, 1);
        }

        beginTest("syncChildren: the target's own properties are left alone");
        {
            auto target = makeList({{"10.0.0.1", 0}});
            target.setProperty("name", "modules", nullptr);
            const auto source = makeList({{"10.0.0.1", 0}});

            ananas::Utils::syncChildren(target, source, Key);

            expectEquals(target["name"].toString(), juce::String{"modules"});
        }
    }

private:
    void expectNotifications(const NotificationCounter &counter, const int propertiesChanged, const int childrenAdded, const int childrenRemoved)
    {
        expectEquals(counter.propertiesChanged, propertiesChanged, "property changes");
        expectEquals(counter.childrenAdded, childrenAdded, "children added");
        expectEquals(counter.childrenRemoved, childrenRemoved, "children removed");
    }
};

static ValueTreeUtilsTests valueTreeUtilsTests;
