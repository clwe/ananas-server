#ifndef WFSLOOKANDFEEL_H
#define WFSLOOKANDFEEL_H

#include <AnanasLookAndFeel.h>
#include "SpeakerIconComponent.h"
#include "XYControllerComponent.h"

namespace ananas::WFS::UI
{
    class WfsLookAndFeel : public ananas::UI::AnanasLookAndFeel
    {
    public:
        WfsLookAndFeel();

        void drawXYController(juce::Graphics &g, const XYControllerComponent &xy);

        void drawXYControllerNode(juce::Graphics &g, const XYControllerComponent::Node &node, float intensity) const;

        void drawSpeakerIcon(juce::Graphics &g, const SpeakerIconComponent & s) const;

    private:
        juce::Path createSpeakerIconPath() const;

        virtual float getNodeBorderBrightnessMin() const { return .2f; }

        virtual float getNodeBorderBrightnessMax() const { return .9f; }

        virtual float getNodeBorderSaturationRange() const { return .9f; }

        virtual float getNodeBorderAlpha() const { return .75f; }

        virtual float getNodeBorderThickness() const { return 3.f; }

        virtual float getNodeIndexFontSize() const { return 18.f; }

        virtual float getSpeakerIconCoilStartX() const { return 26.f; }

        virtual float getSpeakerIconCoilStartY() const { return 0.f; }

        virtual float getSpeakerIconCoilWidth() const { return 48.f; }

        virtual float getSpeakerIconConeEndY() const { return 100.f; }

        virtual float getSpeakerIconConeRightX() const { return 100.f; }

        virtual float getSpeakerIconConeLeftX() const { return 0.f; }

        virtual float getSpeakerIconCoilHeight() const { return 25.f; }

        virtual float getSpeakerOutlineThickness() const { return 0.f; }
    };
}

#endif //WFSLOOKANDFEEL_H
