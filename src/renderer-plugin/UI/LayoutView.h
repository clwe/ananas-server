#ifndef LAYOUTVIEW_H
#define LAYOUTVIEW_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "../SpeakerLayout.h"

namespace ananas::WFS::UI
{
    /**
     * Top-down view of a speaker layout, to scale (the same scale on both
     * axes), zoomed to fit the speakers and the listener, times a zoom
     * factor. x is to the right; the array frame's +y
     * (behind the array) is up, so the listener (y < 0) is below the array
     * line.
     *
     * Read-only: it shows speakers and reports clicks on them; whoever owns
     * the layout changes it.
     */
    class LayoutView final : public juce::Component
    {
    public:
        struct ViewSpeaker
        {
            // Index in the layout; shown as number index + 1.
            int index = 0;
            juce::Point<float> position;
            float z = 0.f;
            Direction facing;
            juce::String moduleIp;
            // Worst LayoutProblem::Severity, as an int; -1 if none.
            int severity = -1;
        };

        LayoutView();

        void setSpeakers(std::vector<ViewSpeaker> newSpeakers);

        // Drawn, and fitted, unless drawn elsewhere (e.g. as a draggable node).
        void setListener(std::optional<juce::Point<float>> listenerPosition);

        // An area (metres, array frame) drawn faintly, e.g. where sources can
        // be; not fitted, so a large range doesn't shrink the speakers.
        void setExtraArea(std::optional<juce::Rectangle<float>> area);

        // Relative to fitting the speakers (and listener): 1 fits, 2 shows
        // half as much, ... Clamped to a sensible range.
        void setZoom(float factor);

        [[nodiscard]] float getZoom() const;

        // Called whenever toView() / toMetres() change (size, contents, zoom).
        std::function<void()> onTransformChanged;

        // Speaker indices to highlight.
        void setSelection(const juce::SparseSet<int> &selection);

        // Metres (array frame) to component coordinates, and back.
        [[nodiscard]] juce::Point<float> toView(juce::Point<float> metres) const;
        [[nodiscard]] juce::Point<float> toMetres(juce::Point<float> view) const;

        // Pixels per metre at the current size.
        [[nodiscard]] float getScale() const;

        // Speaker icons are to scale, within limits: their size in pixels.
        [[nodiscard]] float getSpeakerIconSize() const;

        void paint(juce::Graphics &g) override;

        void resized() override;

        void mouseDown(const juce::MouseEvent &event) override;

        // A click on a speaker (by index), or on empty space (-1).
        std::function<void(int index, const juce::ModifierKeys &modifiers)> onSpeakerClicked;

    private:
        void updateTransform();

        [[nodiscard]] juce::Colour getModuleColour(const juce::String &moduleIp) const;

        std::vector<ViewSpeaker> speakers;
        std::optional<juce::Point<float>> listener;
        std::optional<juce::Rectangle<float>> extraArea;
        juce::SparseSet<int> selected;
        // Colours by module, in order of appearance, so they stay stable.
        juce::StringArray moduleOrder;

        // View = metres * scale, flipped in y, plus offset.
        float scale = 1.f;
        juce::Point<float> offset;
        float zoom = 1.f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayoutView)
    };
}

#endif //LAYOUTVIEW_H
