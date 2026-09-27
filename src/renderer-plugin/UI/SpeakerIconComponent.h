#ifndef SPEAKERICONCOMPONENT_H
#define SPEAKERICONCOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>

namespace ananas::WFS::UI
{
    class SpeakerIconComponent final : public juce::Component
    {
    public:
        SpeakerIconComponent();

        //==================================================================

        enum ColourIds
        {
            fillColourId = 0x2001700,
            borderColourId = 0x2001701
        };

        //==================================================================

        void paint(juce::Graphics &g) override;
    };
}

#endif //SPEAKERICONCOMPONENT_H
