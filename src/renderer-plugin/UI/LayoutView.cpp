#include "LayoutView.h"

namespace ananas::WFS::UI
{
    namespace
    {
        // Space around what's shown, in metres.
        constexpr float MarginMetres = .75f;
        // Smallest area shown, so a single speaker isn't zoomed in absurdly.
        constexpr float MinExtentMetres = 3.f;
        // Speaker icons are drawn to scale, this size (their longer side), but
        // never smaller or larger than these many pixels, so they stay
        // visible when zoomed out and don't get huge when zoomed in. 15 cm
        // keeps icons apart in a linear array at 0.2 m spacing.
        constexpr float SpeakerSizeMetres = .15f;
        constexpr float MinSpeakerIconPixels = 8.f;
        constexpr float MaxSpeakerIconPixels = 60.f;
        // A click this many pixels beyond an icon's radius still selects it.
        constexpr float HitTolerance = 3.f;

        const std::array<juce::Colour, 8> ModuleColours{
            juce::Colour{0xff1f77b4}, juce::Colour{0xff2ca02c}, juce::Colour{0xff9467bd}, juce::Colour{0xff8c564b},
            juce::Colour{0xffe377c2}, juce::Colour{0xff17becf}, juce::Colour{0xffbcbd22}, juce::Colour{0xff7f7f7f},
        };

        // Severity::warning, as an int (see LayoutProblem).
        constexpr int WarningSeverity = 1;

        constexpr float MinZoom = .05f;
        constexpr float MaxZoom = 20.f;

        /**
         * A loudspeaker outline: a narrow coil, and a cone that opens towards
         * +y (down on screen), in a 100 x 100 box centred on (0, 0).
         */
        juce::Path createSpeakerIcon()
        {
            juce::Path icon;
            icon.startNewSubPath(-24.f, -50.f);
            icon.lineTo(24.f, -50.f);
            icon.lineTo(24.f, -25.f);
            icon.lineTo(50.f, 50.f);
            icon.lineTo(-50.f, 50.f);
            icon.lineTo(-24.f, -25.f);
            icon.closeSubPath();
            return icon;
        }

        /**
         * The icon at `centre`, turned so its cone opens the way the speaker
         * faces (horizontally; straight up or down keeps the default).
         */
        juce::AffineTransform speakerIconTransform(const juce::Point<float> centre, const Direction &facing,
                                                   const float iconSize)
        {
            // On screen, y points down: the facing (x, y) is (x, -y) there. The
            // icon opens towards (0, 1); rotating by a maps that to
            // (-sin a, cos a).
            auto angle = 0.f;
            if (const auto h = facing.horizontalLength(); h > .1f) {
                angle = std::atan2(-facing.x / h, -facing.y / h);
            }
            return juce::AffineTransform::scale(iconSize / 100.f)
                    .rotated(angle)
                    .translated(centre);
        }
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

    float LayoutView::getSpeakerIconSize() const
    {
        return juce::jlimit(MinSpeakerIconPixels, MaxSpeakerIconPixels, SpeakerSizeMetres * scale);
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

        // Speakers: an icon in the module's colour, turned to where it faces
        // (the cone opens that way), its number and, if raised, its height.
        const auto icon = createSpeakerIcon();
        const auto iconSize = getSpeakerIconSize();
        const auto radius = iconSize / 2.f;
        for (const auto &s: speakers) {
            const auto p = toView(s.position);
            const auto colour = getModuleColour(s.moduleIp);
            const auto circle = juce::Rectangle<float>{iconSize, iconSize}.withCentre(p);
            const auto transform = speakerIconTransform(p, s.facing, iconSize);

            if (selected.contains(s.index)) {
                g.setColour(juce::Colours::lightblue);
                g.fillEllipse(circle.expanded(5.f));
            }

            g.setColour(colour);
            g.fillPath(icon, transform);
            g.setColour(colour.darker(.6f));
            g.strokePath(icon, juce::PathStrokeType{1.f}, transform);

            if (s.severity >= WarningSeverity) {
                g.setColour(juce::Colours::darkorange);
                g.drawEllipse(circle.expanded(3.f), 2.f);
            }

            g.setColour(juce::Colours::black);
            g.setFont(juce::Font{juce::FontOptions{11.f}});
            auto label = juce::String{s.index + 1};
            if (std::abs(s.z) > .01f) label << " (" << (s.z > 0 ? "+" : "") << juce::String{s.z, 1} << " m)";
            g.drawText(label, juce::Rectangle<float>{p.x + radius + 5.f, p.y - 7.f, 80.f, 14.f},
                       juce::Justification::centredLeft, false);
        }

        g.setColour(juce::Colour{0xffcccccc});
        g.drawRect(getLocalBounds());
    }

    void LayoutView::mouseDown(const juce::MouseEvent &event)
    {
        if (!onSpeakerClicked) return;

        auto closest = -1;
        auto closestDistance = getSpeakerIconSize() / 2.f + HitTolerance;
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
