#ifndef WFSCONTROLLERCOMPONENT_H
#define WFSCONTROLLERCOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModuleComponent.h"
#include "XYControllerComponent.h"
#include "../LinearArray.h"
#if SHOW_NO_NETWORK_OVERLAY
#include <OverlayableComponent.h>
#endif

namespace ananas::WFS::UI
{
    class WFSInterfaceComponent final :
#if SHOW_NO_NETWORK_OVERLAY
            public ananas::UI::OverlayableComponent,
#else
            public juce::Component,
#endif
            public juce::ValueTree::Listener,
            public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        WFSInterfaceComponent(int numSources,
                              juce::AudioProcessorValueTreeState &apvts,
                              juce::ValueTree &persistentTreeToListenTo,
                              juce::ValueTree &dynamicTreeToListenTo,
                              juce::HashMap<int, std::atomic<float> *> &sourceAmplitudes,
                              ModuleComponent::SelectionCallback onModuleSelected,
                              LinearArraySetup& linArrSetupRef);

        ~WFSInterfaceComponent() override;

        void paint(juce::Graphics &g) override;

        void resized() override;

        void updateModuleLists();

        void updateArrayLayout(const juce::var &var);

        // The speakers for the XY view, from speakerLayoutNode.
        void updateSpeakers();

        // Each slot selector above its slot's speakers, at the view's zoom.
        void layoutModuleSelectors();

        void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) override;

        void parameterChanged(const juce::String &parameterID, float newValue) override;

        void expandModuleList(int moduleID);

        void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) override;

        void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WFSInterfaceComponent)

        juce::AudioProcessorValueTreeState &state;
        XYControllerComponent xyController;
        juce::OwnedArray<ModuleComponent> modules;
        juce::ComboBox numModulesSelector;
        juce::Label numModulesLabel;
        juce::ToggleButton showModuleSelectorsButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> showModuleSelectorsAttachment;
        juce::Slider speakerSpacingSlider;
        juce::Label speakerSpacingLabel;
        juce::Label arrayWarningLabel;
        // Shown while Ambisonics modules are connected, in a row of their own.
        juce::Label ambisonicsLabel;
        juce::ComboBox ambisonicsInputSelector;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ambisonicsInputAttachment;
        // Distance cues of encoded sources; shown with the input selector.
        juce::ToggleButton distanceDelayButton, distanceGainButton, distanceLowpassButton;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> distanceCueAttachments;
        juce::ValueTree &persistentTree;
        juce::ValueTree &dynamicTree;
        juce::ValueTree modulesNode;
        // The linear-array setup (module per slot), mirrored by the processor.
        juce::ValueTree linearArrayNode;
        // The speakers in use (from the processor), drawn in the XY view;
        // their x positions place the slot selectors in linear-array mode.
        juce::ValueTree speakerLayoutNode;
        std::vector<float> speakerXs;
        ModuleComponent::SelectionCallback onModuleSelected;
        // Number of speakers in each module slot.
        std::vector<int> slotNumSpeakers;
        LinearArraySetup &linearArraySetup;
    };
} // ananas::WFS

#endif //WFSCONTROLLERCOMPONENT_H
