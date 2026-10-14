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

        // Label
        g.setColour(node.findColour(XYControllerComponent::Node::textColourId));
        g.setFont(getNodeIndexFontSize());
        g.drawText(node.getLabel(),
                   node.getLocalBounds(),
                   juce::Justification::centred);
    }
}
