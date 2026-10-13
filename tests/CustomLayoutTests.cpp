#include <CustomLayout.h>
#include <WFSUtils.h>

namespace
{
    using ananas::WFS::Speaker;

    // Counts the change messages a layout sends.
    struct ChangeCounter final : juce::ChangeListener
    {
        explicit ChangeCounter(ananas::WFS::CustomLayout &l) : layout(l) { layout.addChangeListener(this); }

        ~ChangeCounter() override { layout.removeChangeListener(this); }

        void changeListenerCallback(juce::ChangeBroadcaster *) override { ++count; }

        // Change messages are asynchronous; deliver any pending one now.
        int deliver()
        {
            layout.dispatchPendingMessages();
            return count;
        }

        ananas::WFS::CustomLayout &layout;
        int count = 0;
    };

    const std::vector<Speaker> &speakersOf(const ananas::WFS::CustomLayout &layout)
    {
        return layout.getLayout().getSpeakers();
    }
}

class CustomLayoutTests final : public juce::UnitTest
{
public:
    CustomLayoutTests() : UnitTest("CustomLayout", "Renderer") {}

    void runTest() override
    {
        using ananas::WFS::CustomLayout;

        beginTest("Starts empty");
        {
            const CustomLayout layout;
            expect(speakersOf(layout).empty());
        }

        beginTest("Add, edit and remove speakers");
        {
            CustomLayout layout;

            expectEquals(layout.addSpeaker({{-1.f, 0.f}, "a", 0}), 0);
            expectEquals(layout.addSpeaker(), 1);
            expectEquals(layout.addSpeaker({{1.f, 0.f}, "a", 1}), 2);
            expectEquals(static_cast<int>(speakersOf(layout).size()), 3);

            layout.setPosition(1, {0.f, -.5f});
            layout.setModule(1, "b", 3);
            expect(speakersOf(layout)[1] == Speaker{{0.f, -.5f}, "b", 3});

            layout.removeSpeaker(0);
            expectEquals(static_cast<int>(speakersOf(layout).size()), 2);
            expect(speakersOf(layout)[0] == Speaker{{0.f, -.5f}, "b", 3});
            expect(speakersOf(layout)[1] == Speaker{{1.f, 0.f}, "a", 1});
        }

        beginTest("Indices out of range are ignored");
        {
            CustomLayout layout;
            layout.addSpeaker({{1.f, 0.f}, "a", 0});

            layout.setPosition(1, {5.f, 5.f});
            layout.setPosition(-1, {5.f, 5.f});
            layout.setModule(3, "b", 1);
            layout.removeSpeaker(1);

            expectEquals(static_cast<int>(speakersOf(layout).size()), 1);
            expect(speakersOf(layout)[0] == Speaker{{1.f, 0.f}, "a", 0});
        }

        beginTest("A speaker without a module has output -1");
        {
            CustomLayout layout;
            layout.addSpeaker({{0.f, 0.f}, {}, 4});
            expectEquals(speakersOf(layout)[0].output, -1);

            layout.setModule(0, "a", 2);
            layout.setModule(0, {}, 2);
            expectEquals(speakersOf(layout)[0].output, -1);
        }

        beginTest("Positions that aren't finite become 0");
        {
            CustomLayout layout;
            layout.addSpeaker();
            layout.setPosition(0, {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()});
            expect(speakersOf(layout)[0].position == juce::Point<float>{});
        }

        beginTest("Outputs a module may not have are allowed; checkLayout reports them");
        {
            CustomLayout layout;
            layout.addSpeaker({{0.f, 0.f}, "stereo", 7});
            expectEquals(speakersOf(layout)[0].output, 7);
        }

        beginTest("Replacing all speakers");
        {
            CustomLayout layout;
            layout.addSpeaker();
            layout.setSpeakers({{{-.2f, 0.f}, "a", 0}, {{.2f, 0.f}, "a", 1}});

            expectEquals(static_cast<int>(speakersOf(layout).size()), 2);
            expectEquals(speakersOf(layout)[1].output, 1);
        }

        beginTest("Save and restore");
        {
            CustomLayout layout;
            layout.setSpeakers({{{-.2f, 0.f}, "a", 0}, {{.2f, -1.f}, {}, -1}, {{.6f, 0.f}, "b", 15}});

            CustomLayout restored;
            restored.fromValueTree(layout.toValueTree());

            expect(speakersOf(restored) == speakersOf(layout));
            expect(restored.toValueTree().isEquivalentTo(layout.toValueTree()));
        }

        beginTest("Restoring orders speakers by index and applies the same rules");
        {
            using ananas::WFS::Identifiers;
            juce::ValueTree tree(Identifiers::CustomLayoutTreeType);

            const auto addSpeaker = [&tree](const int index, const float x, const juce::String &ip, const int output)
            {
                juce::ValueTree s(Identifiers::SpeakerTreeType);
                s.setProperty(Identifiers::SpeakerIndexPropertyID, index, nullptr);
                s.setProperty(Identifiers::SpeakerXPropertyID, x, nullptr);
                s.setProperty(Identifiers::SpeakerYPropertyID, 0.f, nullptr);
                s.setProperty(Identifiers::SpeakerModuleIpPropertyID, ip, nullptr);
                s.setProperty(Identifiers::SpeakerOutputPropertyID, output, nullptr);
                tree.addChild(s, -1, nullptr);
            };
            addSpeaker(1, .2f, {}, 3);     // no module: output becomes -1
            addSpeaker(0, -.2f, "a", 0);
            tree.addChild(juce::ValueTree{"NotASpeaker"}, -1, nullptr);

            CustomLayout layout;
            layout.fromValueTree(tree);

            expectEquals(static_cast<int>(speakersOf(layout).size()), 2);
            expect(speakersOf(layout)[0] == Speaker{{-.2f, 0.f}, "a", 0});
            expect(speakersOf(layout)[1] == Speaker{{.2f, 0.f}, {}, -1});
        }

        beginTest("A tree of another type is ignored");
        {
            CustomLayout layout;
            layout.addSpeaker({{1.f, 0.f}, "a", 0});

            layout.fromValueTree(juce::ValueTree{"Something"});
            expectEquals(static_cast<int>(speakersOf(layout).size()), 1);
        }

        beginTest("Change messages only for real changes");
        {
            CustomLayout layout;
            ChangeCounter counter(layout);

            layout.setSpeakers({});
            layout.removeSpeaker(0);
            expectEquals(counter.deliver(), 0);

            layout.addSpeaker({{1.f, 0.f}, "a", 0});
            expectEquals(counter.deliver(), 1);

            layout.setPosition(0, {1.f, 0.f});
            layout.setModule(0, "a", 0);
            expectEquals(counter.deliver(), 1);

            layout.setModule(0, "a", 1);
            expectEquals(counter.deliver(), 2);
        }

        beginTest("Facing directions from and to angles");
        {
            using ananas::WFS::Direction;
            const auto towardsListener = Direction::fromAngles(0.f, 0.f);
            expectWithinAbsoluteError(towardsListener.y, -1.f, 1e-6f);
            // Counter-clockwise seen from above: 90 degrees faces +x.
            const auto right = Direction::fromAngles(90.f, 0.f);
            expectWithinAbsoluteError(right.x, 1.f, 1e-6f);
            expectWithinAbsoluteError(right.y, 0.f, 1e-6f);
            const auto up = Direction::fromAngles(0.f, 90.f);
            expectWithinAbsoluteError(up.z, 1.f, 1e-6f);
            expectWithinAbsoluteError(up.horizontalLength(), 0.f, 1e-6f);

            const auto d = Direction::fromAngles(-135.f, 20.f);
            expectWithinAbsoluteError(d.facingDegrees(), -135.f, 1e-3f);
            expectWithinAbsoluteError(d.tiltDegrees(), 20.f, 1e-3f);

            // No length, or not finite: the default direction.
            expect(Direction{0.f, 0.f, 0.f}.normalised() == Direction{});
            expect(Direction{std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f}.normalised() == Direction{});
        }

        beginTest("Height and facing: edited, kept valid, saved");
        {
            using ananas::WFS::Direction;
            CustomLayout layout;
            layout.addSpeaker();
            expect(speakersOf(layout)[0].facing == Direction{});

            layout.setHeight(0, 1.5f);
            layout.setFacing(0, {2.f, 0.f, 0.f});
            expect(juce::exactlyEqual(speakersOf(layout)[0].z, 1.5f));
            expect(speakersOf(layout)[0].facing == Direction{1.f, 0.f, 0.f});

            layout.setHeight(0, std::numeric_limits<float>::infinity());
            layout.setFacing(0, {0.f, 0.f, 0.f});
            expect(juce::exactlyEqual(speakersOf(layout)[0].z, 0.f));
            expect(speakersOf(layout)[0].facing == Direction{});

            layout.setHeight(0, 2.f);
            layout.setFacing(0, Direction::fromAngles(45.f, -30.f));
            CustomLayout restored;
            restored.fromValueTree(layout.toValueTree());
            expect(speakersOf(restored) == speakersOf(layout));
        }

        beginTest("Layout mode as saved text; unknown text means the linear array");
        {
            using ananas::WFS::LayoutMode;
            expect(ananas::WFS::layoutModeFromString(ananas::WFS::toString(LayoutMode::custom)) == LayoutMode::custom);
            expect(ananas::WFS::layoutModeFromString(ananas::WFS::toString(LayoutMode::linearArray)) == LayoutMode::linearArray);
            expect(ananas::WFS::layoutModeFromString({}) == LayoutMode::linearArray);
            expect(ananas::WFS::layoutModeFromString("ring") == LayoutMode::linearArray);
        }
    }
};

static CustomLayoutTests customLayoutTests;
