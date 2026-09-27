#include "WfsLookAndFeel.h"

namespace ananas::WFS::UI
{
    WfsLookAndFeel::WfsLookAndFeel()
    {
        // Tabbed component
        setColour(juce::TabbedComponent::backgroundColourId, juce::Colours::whitesmoke);
        setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentWhite);
        setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentWhite);
        setColour(juce::TabbedButtonBar::tabTextColourId, darkColour.brighter(.5));
        setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentWhite);
        setColour(juce::TabbedButtonBar::frontTextColourId, darkColour);

        // XY controller
        setColour(XYControllerComponent::backgroundColourId, juce::Colours::black.withAlpha(0.1f));
        setColour(XYControllerComponent::gridLineColourId, juce::Colours::black.withAlpha(0.15f));

        // XY controller nodes
        setColour(XYControllerComponent::Node::backgroundColourId, juce::Colours::white.withAlpha(0.5f));
        setColour(XYControllerComponent::Node::borderColourId, juce::Colours::lightseagreen);
        setColour(XYControllerComponent::Node::textColourId, darkColour);

        // Speaker icons
        setColour(SpeakerIconComponent::fillColourId, juce::Colours::lightgrey);
        setColour(SpeakerIconComponent::borderColourId, juce::Colours::darkgrey);
    }

    void WfsLookAndFeel::drawXYController(juce::Graphics &g, const XYControllerComponent &xy)
    {
        g.fillAll(xy.findColour(XYControllerComponent::backgroundColourId));

        g.setColour(xy.findColour(XYControllerComponent::gridLineColourId));

        const auto bounds{xy.getBounds().toFloat()};
        const auto right{bounds.getRight()};

        if (xy.getGridSpacingX() > 0) {
            // Draw positive x grid lines, one per metre
            for (int x{xy.getWidth() / 2}; x < xy.getWidth(); x += xy.getGridSpacingX()) {
                g.drawVerticalLine(x, 0.f, bounds.getHeight());
            }

            // Draw negative x grid lines
            for (int x{xy.getWidth() / 2 - xy.getGridSpacingX()}; x > 0; x -= xy.getGridSpacingX()) {
                g.drawVerticalLine(x, 0.f, bounds.getHeight());
            }
        }

        const auto yZero{xy.getMaxYMetres() * xy.getHeight() / (xy.getMaxYMetres() - xy.getMinYMetres())};
        const auto unit{xy.getHeight() / (xy.getMaxYMetres() - xy.getMinYMetres())};

        // y gridlines
        for (int y{xy.getMinYMetres()}; y < xy.getMaxYMetres(); ++y) {
            g.drawHorizontalLine(yZero - y * unit, 0.f, right);
        }
    }

    void WfsLookAndFeel::drawXYControllerNode(juce::Graphics &g, const XYControllerComponent::Node &node, const float intensity) const
    {
        const auto bounds{node.getLocalBounds().toFloat()};

        // Background
        g.setColour(node.findColour(XYControllerComponent::Node::backgroundColourId));
        g.fillEllipse(bounds);

        // Border
        const auto level{(100.f + intensity) * .01f};
        const auto baseBorderColour{node.findColour(XYControllerComponent::Node::borderColourId)};

        const auto saturation{1.0f - level * getNodeBorderSaturationRange()};
        const auto brightness{getNodeBorderBrightnessMin() + level * getNodeBorderBrightnessMax()};

        g.setColour(juce::Colour::fromHSV(
            baseBorderColour.getHue(),
            saturation,
            brightness,
            getNodeBorderAlpha()));

        const auto borderThickness{getNodeBorderThickness()};
        g.drawEllipse(bounds.withSizeKeepingCentre(
                          bounds.getWidth() - borderThickness,
                          bounds.getHeight() - borderThickness),
                      borderThickness);

        // Index
        g.setColour(node.findColour(XYControllerComponent::Node::textColourId));
        g.setFont(getNodeIndexFontSize());
        g.drawText(juce::String{node.getIndex() + 1},
                   node.getLocalBounds(),
                   juce::Justification::centred);
    }

    void WfsLookAndFeel::drawSpeakerIcon(juce::Graphics &g, const SpeakerIconComponent & s) const
    {
        const auto bounds{s.getBounds().toFloat()};
        const auto speaker{createSpeakerIconPath()};

        const auto targetBounds{
            juce::Rectangle{
                0.f, 0.f,
                bounds.getWidth(),
                bounds.getHeight() - getSpeakerOutlineThickness()
            }
        };

        const juce::RectanglePlacement placement{juce::RectanglePlacement::stretchToFit};
        const auto transform{
            placement.getTransformToFit(speaker.getBounds(), targetBounds)
        };

        g.setColour(s.findColour(SpeakerIconComponent::fillColourId));
        g.fillPath(speaker, transform);

        g.setColour(s.findColour(SpeakerIconComponent::borderColourId));
        g.strokePath(speaker, juce::PathStrokeType{getSpeakerOutlineThickness(), juce::PathStrokeType::mitered}, transform);
    }

    juce::Path WfsLookAndFeel::createSpeakerIconPath() const
    {
        juce::Path speaker;

        speaker.startNewSubPath(getSpeakerIconCoilStartX(), getSpeakerIconCoilStartY());
        speaker.lineTo(getSpeakerIconCoilStartX() + getSpeakerIconCoilWidth(), getSpeakerIconCoilStartY());
        speaker.lineTo(getSpeakerIconCoilStartX() + getSpeakerIconCoilWidth(), getSpeakerIconCoilHeight());
        speaker.lineTo(getSpeakerIconConeRightX(), getSpeakerIconConeEndY());
        speaker.lineTo(getSpeakerIconConeLeftX(), getSpeakerIconConeEndY());
        speaker.lineTo(getSpeakerIconCoilStartX(), getSpeakerIconCoilHeight());
        speaker.closeSubPath();

        return speaker;
    }
}
