#ifndef NETWORKOVERVIEWCOMPONENT_H
#define NETWORKOVERVIEWCOMPONENT_H

#include "SwitchesComponent.h"
#include "TimeAuthorityComponent.h"
#include "ClientsOverviewComponent.h"
#if SHOW_NO_NETWORK_OVERLAY
#include "OverlayableComponent.h"
#endif

namespace ananas::UI
{
    class NetworkOverviewComponent final :
#if SHOW_NO_NETWORK_OVERLAY
            public OverlayableComponent
#else
            public juce::Component
#endif
    {
    public:
        NetworkOverviewComponent(juce::ValueTree &dynamicTree, juce::ValueTree &persistentTree, SwitchList &switchList);

        //======================================================================

        enum ColourIds
        {
            backgroundColourId = 0x2001100,
        };

        //======================================================================

        void paint(juce::Graphics &g) override;

        void resized() override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NetworkOverviewComponent)

        SwitchesComponent switches;
        TimeAuthorityComponent timeAuthority;
        ClientsOverviewComponent clientOverview;
    };
}


#endif //NETWORKOVERVIEWCOMPONENT_H
