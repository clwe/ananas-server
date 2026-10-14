#ifndef WFSLOOKANDFEEL_H
#define WFSLOOKANDFEEL_H

#include <AnanasLookAndFeel.h>
#include "XYControllerComponent.h"

namespace ananas::WFS::UI
{
    class WfsLookAndFeel : public ananas::UI::AnanasLookAndFeel
    {
    public:
        WfsLookAndFeel();

        void drawXYControllerNode(juce::Graphics &g, const XYControllerComponent::Node &node, float intensity) const;

    private:
        virtual float getNodeBorderBrightnessMin() const { return .2f; }

        virtual float getNodeBorderBrightnessMax() const { return .9f; }

        virtual float getNodeBorderSaturationRange() const { return .9f; }

        virtual float getNodeBorderAlpha() const { return .75f; }

        virtual float getNodeBorderThickness() const { return 3.f; }

        virtual float getNodeIndexFontSize() const { return 18.f; }
    };
}

#endif //WFSLOOKANDFEEL_H
