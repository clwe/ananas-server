#include "WFSInterfaceComponent.h"
#include <AnanasUtils.h>
#include <Server.h>

#include "../WFSUtils.h"
#include <numeric>

namespace ananas::WFS::UI
{
    WFSInterfaceComponent::WFSInterfaceComponent(
        const int numSources,
        juce::AudioProcessorValueTreeState &apvts,
        juce::ValueTree &persistentTreeToListenTo,
        juce::ValueTree &dynamicTreeToListenTo,
        juce::HashMap<int, std::atomic<float> *> &sourceAmplitudes,
        ModuleComponent::SelectionCallback onModuleSelectedCallback
    ) : state(apvts),
        xyController(numSources, apvts, sourceAmplitudes),
        persistentTree(persistentTreeToListenTo),
        dynamicTree(dynamicTreeToListenTo),
        onModuleSelected(std::move(onModuleSelectedCallback))
    {
        // Display the XY-controller
        addAndMakeVisible(xyController);

        // Display and attach the number-of-modules selector.
        addAndMakeVisible(numModulesLabel);
        numModulesLabel.attachToComponent(&numModulesSelector, true);
        numModulesLabel.setText(Params::NumModules.name, juce::dontSendNotification);
        numModulesLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(numModulesSelector);
        for (size_t n{0}; n < Constants::MaxNumModules; ++n) {
            numModulesSelector.addItem(juce::String{n + 1}, n + 1);
        }

        numModulesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state,
            Params::NumModules.id,
            numModulesSelector
        );

        // Display and attach the speaker spacing selector.
        addAndMakeVisible(speakerSpacingLabel);
        speakerSpacingLabel.attachToComponent(&speakerSpacingSlider, true);
        speakerSpacingLabel.setText(Params::SpeakerSpacing.name, juce::dontSendNotification);
        speakerSpacingLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(speakerSpacingSlider);
        speakerSpacingSlider.setSliderStyle(juce::Slider::SliderStyle::IncDecButtons);
        speakerSpacingSlider.setNormalisableRange(Params::SpeakerSpacing.rangeDouble);
        speakerSpacingSlider.setValue(Params::SpeakerSpacing.defaultValue);
        speakerSpacingSlider.setTextBoxStyle(juce::Slider::TextBoxLeft,
                                             false,
                                             speakerSpacingSlider.getTextBoxWidth(),
                                             25);
        speakerSpacingSlider.setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_Vertical);

        speakerSpacingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            state,
            Params::SpeakerSpacing.id,
            speakerSpacingSlider
        );

        // Display and attach the show-module-selectors checkbox.
        addAndMakeVisible(showModuleSelectorsButton);
        showModuleSelectorsButton.setButtonText(Params::ShowModuleSelectors.name);

        showModuleSelectorsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            state,
            Params::ShowModuleSelectors.id,
            showModuleSelectorsButton
        );

        // Shown when the array is limited, e.g. by modules that render fewer
        // sources than the plugin provides.
        addAndMakeVisible(arrayWarningLabel);
        arrayWarningLabel.setJustificationType(juce::Justification::centredLeft);
        arrayWarningLabel.setColour(juce::Label::textColourId, juce::Colours::darkorange.darker(.3f));

        // Listen for changes to module selector display state.
        state.addParameterListener(Params::ShowModuleSelectors.id, this);

        // Listen to the dynamic tree for changes to the array layout (number
        // of modules, speakers per module, spacing), and set up the modules.
        dynamicTree.addListener(this);
        updateArrayLayout(dynamicTree[Identifiers::ArrayLayoutParamID]);

        // Listen to the persistent tree for module selection changes.
        persistentTree.addListener(this);
        // Trigger an initial property change so that combo boxes get populated.
        persistentTree.sendPropertyChangeMessage(ananas::Utils::Identifiers::ModulesParamID);
    }

    WFSInterfaceComponent::~WFSInterfaceComponent()
    {
        state.removeParameterListener(Params::ShowModuleSelectors.id, this);
        dynamicTree.removeListener(this);
        persistentTree.removeListener(this);
        showModuleSelectorsButton.setLookAndFeel(nullptr);
    }

    void WFSInterfaceComponent::paint(juce::Graphics &g)
    {
        g.fillAll(juce::Colours::transparentWhite);
    }

    void WFSInterfaceComponent::resized()
    {
        auto bounds{getLocalBounds()};
        auto optionsRow{
            bounds.removeFromTop(Dimensions::SpeakerSpacingSectionHeight)
            .reduced(6, 0)
        };
        speakerSpacingSlider.setBounds(optionsRow.removeFromRight(100).reduced(1, 3));
        speakerSpacingLabel.setBounds(optionsRow.removeFromRight(200));

        numModulesSelector.setBounds(optionsRow.removeFromRight(75).reduced(1, 12));
        numModulesLabel.setBounds(optionsRow.removeFromRight(200));

        showModuleSelectorsButton.setBounds(optionsRow.removeFromLeft(300));
        arrayWarningLabel.setBounds(optionsRow);

        bounds = bounds.reduced(10);
        xyController.setBounds(bounds);

        const auto yZero{Constants::MaxYMetres * bounds.getHeight() / (Constants::MaxYMetres - Constants::MinYMetres)};
        bounds.removeFromTop(yZero - Dimensions::ModuleSelectorHeight - Dimensions::SpeakerIconHeight);

        juce::FlexBox moduleFlex;
        moduleFlex.flexDirection = juce::FlexBox::Direction::row;

        for (int n{0}; n < modules.size(); ++n) {
            // Width in proportion to the number of speakers in the slot.
            const auto numSpeakers{static_cast<size_t>(n) < slotNumSpeakers.size() ? slotNumSpeakers[static_cast<size_t>(n)] : 1};
            moduleFlex.items.add(juce::FlexItem(*modules[n])
                .withFlex(static_cast<float>(numSpeakers))
                .withMaxHeight(Dimensions::ModuleSelectorHeight));
        }

        moduleFlex.performLayout(bounds);

        bounds.removeFromTop(Dimensions::ModuleSelectorHeight);

        juce::FlexBox speakerFlex;
        // Speaker icons
        speakerFlex.flexDirection = juce::FlexBox::Direction::row;
        speakerFlex.justifyContent = juce::FlexBox::JustifyContent::center;
        for (const auto &s: speakerIcons) {
            speakerFlex.items.add(juce::FlexItem{*s}
                .withFlex(1.f)
                .withHeight(Dimensions::SpeakerIconHeight));
        }

        speakerFlex.performLayout(bounds);

#if SHOW_NO_NETWORK_OVERLAY
        OverlayableComponent::resized();
#endif
    }

    void WFSInterfaceComponent::updateModuleLists(const juce::var &var)
    {
        juce::StringArray ips;
        std::map<int, juce::String> slotModules;

        if (auto *obj = var.getDynamicObject()) {
            for (const auto &prop: obj->getProperties()) {
                if (const auto *module = prop.value.getDynamicObject()) {
                    if (module->getProperty(ananas::Utils::Identifiers::ModuleIsConnectedPropertyID)) {
                        ips.add(prop.name.toString());
                    }
                    if (const int slot{module->getProperty(ananas::Utils::Identifiers::ModuleSlotPropertyID)}; slot >= 0) {
                        slotModules[slot] = prop.name.toString();
                    }
                }
            }
        }

        for (int n{0}; n < modules.size(); ++n) {
            const auto iter{slotModules.find(n)};
            modules[n]->setAvailableModules(ips, iter != slotModules.end() ? iter->second : juce::String{});
        }
    }

    void WFSInterfaceComponent::updateArrayLayout(const juce::var &var)
    {
        const auto *layout{var.getDynamicObject()};
        if (layout == nullptr) return;

        slotNumSpeakers.clear();
        if (const auto *slots{layout->getProperty(Identifiers::ArraySlotNumSpeakersPropertyID).getArray()}) {
            for (const auto &n: *slots) {
                slotNumSpeakers.push_back(static_cast<int>(n));
            }
        }

        const auto numSlots{static_cast<int>(slotNumSpeakers.size())};
        const auto totalNumSpeakers{std::accumulate(slotNumSpeakers.begin(), slotNumSpeakers.end(), 0)};

        // Module selectors, one per slot.
        if (modules.size() != numSlots) {
            modules.clear();
            const auto showModuleSelectors{state.getRawParameterValue(Params::ShowModuleSelectors.id)->load() > .5f};
            for (int n{0}; n < numSlots; ++n) {
                const auto m{modules.add(new ModuleComponent(n, onModuleSelected))};
                addAndMakeVisible(m);
                m->setBroughtToFrontOnMouseClick(true);
                m->shouldShowModuleSelector(showModuleSelectors);
            }
            updateModuleLists(persistentTree[ananas::Utils::Identifiers::ModulesParamID]);
        }

        // Speaker icons, one per speaker.
        if (speakerIcons.size() != totalNumSpeakers) {
            speakerIcons.clear();
            for (int n{0}; n < totalNumSpeakers; ++n) {
                const auto s{speakerIcons.add(new SpeakerIconComponent)};
                addAndMakeVisible(s, -1);
            }
        }

        const int numRenderedSources{layout->getProperty(Identifiers::ArrayNumRenderedSourcesPropertyID)};
        const int numLimitingModules{layout->getProperty(Identifiers::ArrayNumLimitingModulesPropertyID)};
        const float outerSpeakerX{layout->getProperty(Identifiers::ArrayOuterSpeakerXPropertyID)};

        xyController.setArrayGeometry(layout->getProperty(Identifiers::ArrayWidthPropertyID), numRenderedSources);

        juce::StringArray warnings;

        if (numLimitingModules > 0 && numRenderedSources < static_cast<int>(Constants::NumSources)) {
            const auto first{numRenderedSources + 1}, last{static_cast<int>(Constants::NumSources)};
            warnings.add((first == last ? "Source " + juce::String{first} + " is"
                                        : "Sources " + juce::String{first} + juce::String::fromUTF8("\u2013") + juce::String{last} + " are") +
                         " silent on " + juce::String{numLimitingModules} +
                         (numLimitingModules == 1 ? " module." : " modules."));
        }

        if (outerSpeakerX > Constants::MaxXMetres) {
            warnings.add("Outer speakers at " + juce::String::fromUTF8("\u00b1") + juce::String{outerSpeakerX, 2} +
                         " m exceed the modules' range of " + juce::String::fromUTF8("\u00b1") +
                         juce::String{Constants::MaxXMetres, 2} + " m.");
        }

        arrayWarningLabel.setText(warnings.joinIntoString(" "), juce::dontSendNotification);
        arrayWarningLabel.setTooltip(arrayWarningLabel.getText());

        resized();
    }

    void WFSInterfaceComponent::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property)
    {
        // if (!isVisible()) return;

        if (property == ananas::Utils::Identifiers::ModulesParamID) {
            updateModuleLists(treeWhosePropertyHasChanged[property]);
        } else if (property == Identifiers::ArrayLayoutParamID) {
            updateArrayLayout(treeWhosePropertyHasChanged[property]);
        }
    }

    void WFSInterfaceComponent::parameterChanged(const juce::String &parameterID, const float newValue)
    {
        if (parameterID == Params::ShowModuleSelectors.id) {
            const auto show{newValue > .5f};
            for (auto *m: modules) {
                m->shouldShowModuleSelector(show);
            }
        }

        resized();
    }

    void WFSInterfaceComponent::expandModuleList(const int moduleID)
    {
        if (moduleID < 0 || moduleID >= modules.size()) return;

        for (auto *m: modules) {
            m->collapseModuleList();
        }
        modules[moduleID]->expandModuleList();
    }
} // ananas::WFS
