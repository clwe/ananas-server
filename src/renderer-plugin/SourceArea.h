#ifndef SOURCEAREA_H
#define SOURCEAREA_H

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

namespace ananas::WFS
{
    /**
     * Where sources can be placed, in metres: a rectangle in the array frame,
     * independent of the speakers. The source position parameters are
     * normalised (-1 ... 1 on each axis) and map linearly onto it, for every
     * renderer and the UI alike.
     */
    struct SourceArea
    {
        float xFrom = -5.f;
        float xTo = 5.f;
        float yFrom = -5.f;
        float yTo = 5.f;

        // A normalised source position to metres.
        [[nodiscard]] juce::Point<float> toMetres(juce::Point<float> normalised) const;

        // Metres to a normalised source position, clamped to the area.
        [[nodiscard]] juce::Point<float> toNormalised(juce::Point<float> metres) const;

        [[nodiscard]] juce::Rectangle<float> getBounds() const;
    };

    bool operator==(const SourceArea &a, const SourceArea &b);

    bool operator!=(const SourceArea &a, const SourceArea &b);

    /**
     * A SourceArea that one thread can update while others (e.g. the audio
     * thread) read it. The four values are atomic each; a read during an
     * update may mix old and new values, which is harmless for one block.
     */
    class AtomicSourceArea
    {
    public:
        void store(const SourceArea &area);

        [[nodiscard]] SourceArea load() const;

    private:
        std::atomic<float> xFrom{SourceArea{}.xFrom};
        std::atomic<float> xTo{SourceArea{}.xTo};
        std::atomic<float> yFrom{SourceArea{}.yFrom};
        std::atomic<float> yTo{SourceArea{}.yTo};
    };

    /**
     * The plugin's source area: the one live instance, changed only through
     * its methods, which keep it valid and notify listeners.
     *
     * Always valid: every edge within ±Constants::MaxPositionMetres (what WFS
     * modules accept), and each axis at least
     * Constants::MinSourceAreaSizeMetres long, `from` below `to`.
     */
    class SourceAreaSetup final : public juce::ChangeBroadcaster
    {
    public:
        SourceAreaSetup() = default;

        [[nodiscard]] const SourceArea &getArea() const;

        void setX(float from, float to);

        void setY(float from, float to);

        [[nodiscard]] juce::ValueTree toValueTree() const;

        // Applies the same rules as the setters; missing values keep the
        // defaults.
        void fromValueTree(const juce::ValueTree &tree);

    private:
        // Applies the rules to `newArea`, and notifies listeners if that
        // changes anything.
        void apply(SourceArea newArea);

        SourceArea area;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SourceAreaSetup)
    };
}

#endif //SOURCEAREA_H
