#include "LayoutView.h"

namespace ananas::WFS::UI
{
    namespace
    {
        // Space around what's shown, in metres.
        constexpr float MarginMetres = .75f;
        // Smallest area shown, so a single speaker isn't zoomed in absurdly.
        constexpr float MinExtentMetres = 3.f;
        // Speaker symbol radius and facing arrow length, in pixels.
        constexpr float SpeakerRadius = 7.f;
        constexpr float ArrowLength = 16.f;
        // A click within this many pixels of a speaker selects it.
        constexpr float HitRadius = 11.f;

        const std::array<juce::Colour, 8> ModuleColours{
            juce::Colour{0xff1f77b4}, juce::Colour{0xff2ca02c}, juce::Colour{0xff9467bd}, juce::Colour{0xff8c564b},
            juce::Colour{0xffe377c2}, juce::Colour{0xff17becf}, juce::Colour{0xffbcbd22}, juce::Colour{0xff7f7f7f},
        };

        // Severity::warning, as an int (see LayoutProblem).
        constexpr int WarningSeverity = 1;

        constexpr float MinZoom = .05f;
        constexpr float MaxZoom = 20.f;
    }

    LayoutView::LayoutView()
    {
        setOpaque(true);
    }

    void LayoutView::setSpeakers(std::vector<ViewSpeaker> newSpeakers)
    {
        speakers = std::move(newSpeakers);
        for (const auto &s: speakers) {
            if (s.moduleIp.isNotEmpty()) moduleOrder.addIfNotAlreadyThere(s.moduleIp);
        }
        updateTransform();
        repaint();
    }

    void LayoutView::setListener(const std::optional<juce::Point<float>> listenerPosition)
    {
        if (listenerPosition == listener) return;
        listener = listenerPosition;
        updateTransform();
        repaint();
    }

    void LayoutView::setExtraArea(const std::optional<juce::Rectangle<float>> area)
    {
        if (area == extraArea) return;
        extraArea = area;
        updateTransform();
        repaint();
    }

    void LayoutView::setZoom(const float factor)
    {
        const auto clamped = juce::jlimit(MinZoom, MaxZoom, factor);
        if (juce::exactlyEqual(clamped, zoom)) return;
        zoom = clamped;
        updateTransform();
        repaint();
    }

    float LayoutView::getZoom() const
    {
        return zoom;
    }

    void LayoutView::setSelection(const juce::SparseSet<int> &selection)
    {
        if (selection == selected) return;
        selected = selection;
        repaint();
    }

    juce::Point<float> LayoutView::toView(const juce::Point<float> metres) const
    {
        return {offset.x + metres.x * scale, offset.y - metres.y * scale};
    }

    juce::Point<float> LayoutView::toMetres(const juce::Point<float> view) const
    {
        return {(view.x - offset.x) / scale, (offset.y - view.y) / scale};
    }

    float LayoutView::getScale() const
    {
        return scale;
    }

    void LayoutView::resized()
    {
        updateTransform();
    }

    void LayoutView::updateTransform()
    {
        // What has to fit: the array line's centre, the speakers, the
        // listener and the extra area. (Not Rectangle::getUnion: it ignores
        // empty rectangles, which a single point is.)
        auto minimum = juce::Point<float>{0.f, 0.f};
        auto maximum = minimum;
        const auto include = [&minimum, &maximum](const juce::Point<float> p)
        {
            minimum = {juce::jmin(minimum.x, p.x), juce::jmin(minimum.y, p.y)};
            maximum = {juce::jmax(maximum.x, p.x), juce::jmax(maximum.y, p.y)};
        };
        for (const auto &s: speakers) include(s.position);
        if (listener) include(*listener);

        auto bounds = juce::Rectangle<float>{minimum, maximum}.expanded(MarginMetres);
        bounds = bounds.withSizeKeepingCentre(juce::jmax(bounds.getWidth(), MinExtentMetres),
                                              juce::jmax(bounds.getHeight(), MinExtentMetres));

        const auto area = getLocalBounds().toFloat();
        if (area.isEmpty()) return;

        // The same scale on both axes: whichever direction is tighter.
        const auto newScale = zoom * juce::jmin(area.getWidth() / bounds.getWidth(), area.getHeight() / bounds.getHeight());

        // Centre the bounds; y is flipped (+y up).
        const juce::Point<float> newOffset{area.getCentreX() - bounds.getCentreX() * newScale,
                                           area.getCentreY() + bounds.getCentreY() * newScale};

        if (juce::exactlyEqual(newScale, scale) && newOffset == offset) return;
        scale = newScale;
        offset = newOffset;
        if (onTransformChanged) onTransformChanged();
    }

    juce::Colour LayoutView::getModuleColour(const juce::String &moduleIp) const
    {
        const auto i = moduleOrder.indexOf(moduleIp);
        return i < 0 ? juce::Colours::grey : ModuleColours[static_cast<size_t>(i) % ModuleColours.size()];
    }

    void LayoutView::paint(juce::Graphics &g)
    {
        g.fillAll(juce::Colours::white);

        // Metre grid, over the whole component.
        const auto topLeft = toMetres({0.f, 0.f});
        const auto bottomRight = toMetres({static_cast<float>(getWidth()), static_cast<float>(getHeight())});
        g.setColour(juce::Colour{0xffeeeeee});
        for (auto x = std::ceil(topLeft.x); x <= bottomRight.x; x += 1.f) {
            g.drawVerticalLine(juce::roundToInt(toView({x, 0.f}).x), 0.f, static_cast<float>(getHeight()));
        }
        for (auto y = std::floor(topLeft.y); y >= bottomRight.y; y -= 1.f) {
            g.drawHorizontalLine(juce::roundToInt(toView({0.f, y}).y), 0.f, static_cast<float>(getWidth()));
        }

        // Where sources can be.
        if (extraArea) {
            const auto a = juce::Rectangle<float>{toView(extraArea->getTopLeft()), toView(extraArea->getBottomRight())};
            g.setColour(juce::Colour{0x0d000000});
            g.fillRect(a);
            g.setColour(juce::Colour{0x30000000});
            g.drawRect(a, 1.f);
        }

        // The array line, y = 0, and the scale.
        g.setColour(juce::Colour{0xffbbbbbb});
        g.drawHorizontalLine(juce::roundToInt(toView({0.f, 0.f}).y), 0.f, static_cast<float>(getWidth()));
        g.setColour(juce::Colour{0xff999999});
        g.setFont(juce::Font{juce::FontOptions{11.f}});
        g.drawText("grid: 1 m", getLocalBounds().reduced(4), juce::Justification::bottomRight);

        // The listener.
        if (listener) {
            const auto p = toView(*listener);
            g.setColour(juce::Colours::darkorange);
            g.fillEllipse(juce::Rectangle<float>{12.f, 12.f}.withCentre(p));
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font{juce::FontOptions{10.f, juce::Font::bold}});
            g.drawText("L", juce::Rectangle<float>{12.f, 12.f}.withCentre(p), juce::Justification::centred);
        }

        // Speakers: a symbol in the module's colour, an arrow for where it
        // faces (horizontally), its number and, if raised, its height.
        for (const auto &s: speakers) {
            const auto p = toView(s.position);
            const auto colour = getModuleColour(s.moduleIp);
            const auto circle = juce::Rectangle<float>{2.f * SpeakerRadius, 2.f * SpeakerRadius}.withCentre(p);

            if (s.facing.horizontalLength() > .1f) {
                const auto h = s.facing.horizontalLength();
                const juce::Point<float> tip{p.x + ArrowLength * s.facing.x / h, p.y - ArrowLength * s.facing.y / h};
                g.setColour(colour.withAlpha(.8f));
                g.drawArrow(juce::Line<float>{p, tip}, 1.5f, 6.f, 6.f);
            }

            if (selected.contains(s.index)) {
                g.setColour(juce::Colours::lightblue);
                g.fillEllipse(circle.expanded(4.f));
            }

            g.setColour(colour);
            g.fillEllipse(circle);
            if (s.severity >= WarningSeverity) {
                g.setColour(juce::Colours::darkorange);
                g.drawEllipse(circle.expanded(1.5f), 2.f);
            }

            g.setColour(juce::Colours::black);
            g.setFont(juce::Font{juce::FontOptions{11.f}});
            auto label = juce::String{s.index + 1};
            if (std::abs(s.z) > .01f) label << " (" << (s.z > 0 ? "+" : "") << juce::String{s.z, 1} << " m)";
            g.drawText(label, juce::Rectangle<float>{p.x + SpeakerRadius + 2.f, p.y - 7.f, 80.f, 14.f},
                       juce::Justification::centredLeft, false);
        }

        g.setColour(juce::Colour{0xffcccccc});
        g.drawRect(getLocalBounds());
    }

    void LayoutView::mouseDown(const juce::MouseEvent &event)
    {
        if (!onSpeakerClicked) return;

        auto closest = -1;
        auto closestDistance = HitRadius;
        for (const auto &s: speakers) {
            const auto d = toView(s.position).getDistanceFrom(event.position);
            if (d <= closestDistance) {
                closest = s.index;
                closestDistance = d;
            }
        }

        onSpeakerClicked(closest, event.mods);
    }
}
