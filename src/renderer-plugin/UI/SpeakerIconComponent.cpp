#include "SpeakerIconComponent.h"
#include "WfsLookAndFeel.h"

namespace ananas::WFS::UI {
    SpeakerIconComponent::SpeakerIconComponent()
    {
        setOpaque(false);
    }

    void SpeakerIconComponent::paint(juce::Graphics &g)
    {
        if (const auto *lnf{dynamic_cast<WfsLookAndFeel *>(&getLookAndFeel())})
            lnf->drawSpeakerIcon(g, *this);
    }
}
