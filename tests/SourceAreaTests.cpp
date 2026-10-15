#include <SourceArea.h>
#include <WFSUtils.h>

namespace
{
    bool approximatelyEqual(const float a, const float b) { return std::abs(a - b) < 1e-5f; }

    bool approximatelyEqual(const juce::Point<float> a, const juce::Point<float> b)
    {
        return approximatelyEqual(a.x, b.x) && approximatelyEqual(a.y, b.y);
    }

    class ChangeCounter final : public juce::ChangeListener
    {
    public:
        void changeListenerCallback(juce::ChangeBroadcaster *) override { ++count; }

        int count = 0;
    };
}

class SourceAreaTests final : public juce::UnitTest
{
public:
    SourceAreaTests() : UnitTest("SourceArea", "Renderer") {}

    void runTest() override
    {
        using ananas::WFS::SourceArea;
        using ananas::WFS::SourceAreaSetup;
        using ananas::WFS::Constants;

        beginTest("The default is 10 x 10 m around the origin");
        {
            const SourceArea area;
            expect(area.getBounds() == juce::Rectangle<float>{-5.f, -5.f, 10.f, 10.f});
        }

        beginTest("Normalised positions map linearly onto the area, and back");
        {
            const SourceArea area{-1.f, 3.f, 2.f, 4.f};
            expect(approximatelyEqual(area.toMetres({-1.f, -1.f}), {-1.f, 2.f}));
            expect(approximatelyEqual(area.toMetres({1.f, 1.f}), {3.f, 4.f}));
            expect(approximatelyEqual(area.toMetres({0.f, 0.f}), {1.f, 3.f}));
            expect(approximatelyEqual(area.toMetres({.5f, -.5f}), {2.f, 2.5f}));

            expect(approximatelyEqual(area.toNormalised({2.f, 2.5f}), {.5f, -.5f}));
            // Outside the area: clamped to its edge.
            expect(approximatelyEqual(area.toNormalised({10.f, -10.f}), {1.f, -1.f}));
        }

        beginTest("Setters keep the area valid");
        {
            SourceAreaSetup setup;

            setup.setX(-2.5f, 2.5f);
            expect(approximatelyEqual(setup.getArea().xFrom, -2.5f));
            expect(approximatelyEqual(setup.getArea().xTo, 2.5f));

            // Within the modules' range.
            setup.setY(-20.f, 20.f);
            expect(approximatelyEqual(setup.getArea().yFrom, -Constants::MaxPositionMetres));
            expect(approximatelyEqual(setup.getArea().yTo, Constants::MaxPositionMetres));

            // Reversed: swapped.
            setup.setX(3.f, -1.f);
            expect(approximatelyEqual(setup.getArea().xFrom, -1.f));
            expect(approximatelyEqual(setup.getArea().xTo, 3.f));

            // Too small: `to` moves to make it the minimum size.
            setup.setX(1.f, 1.2f);
            expect(approximatelyEqual(setup.getArea().xFrom, 1.f));
            expect(approximatelyEqual(setup.getArea().xTo, 1.f + Constants::MinSourceAreaSizeMetres));

            // Too small at the upper limit: `from` moves instead.
            setup.setY(10.f, 10.f);
            expect(approximatelyEqual(setup.getArea().yFrom, Constants::MaxPositionMetres - Constants::MinSourceAreaSizeMetres));
            expect(approximatelyEqual(setup.getArea().yTo, Constants::MaxPositionMetres));
        }

        beginTest("Listeners are notified of real changes only");
        {
            SourceAreaSetup setup;
            ChangeCounter counter;
            setup.addChangeListener(&counter);

            setup.setX(-5.f, 5.f); // the default
            setup.dispatchPendingMessages();
            expectEquals(counter.count, 0);

            setup.setX(-2.f, 2.f);
            setup.dispatchPendingMessages();
            expectEquals(counter.count, 1);

            setup.removeChangeListener(&counter);
        }

        beginTest("Saved and restored, with the same rules");
        {
            SourceAreaSetup setup;
            setup.setX(-2.5f, 2.5f);
            setup.setY(-1.f, 4.f);

            SourceAreaSetup restored;
            restored.fromValueTree(setup.toValueTree());
            expect(restored.getArea() == setup.getArea());

            auto invalid = setup.toValueTree();
            invalid.setProperty(ananas::WFS::Identifiers::SourceAreaXToPropertyID, 50.f, nullptr);
            invalid.removeProperty(ananas::WFS::Identifiers::SourceAreaYFromPropertyID, nullptr);
            restored.fromValueTree(invalid);
            expect(approximatelyEqual(restored.getArea().xTo, Constants::MaxPositionMetres));
            // Missing: the default.
            expect(approximatelyEqual(restored.getArea().yFrom, SourceArea{}.yFrom));

            // Other trees are ignored.
            restored.fromValueTree(juce::ValueTree{"Something"});
            expect(approximatelyEqual(restored.getArea().xTo, Constants::MaxPositionMetres));
        }
    }
};

static SourceAreaTests sourceAreaTests;
