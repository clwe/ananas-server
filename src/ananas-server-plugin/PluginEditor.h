#ifndef PLUGINEDITOR_H
#define PLUGINEDITOR_H

#include <AnanasLookAndFeel.h>
#include "PluginProcessor.h"
#include <NetworkOverviewComponent.h>

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor &);

    ~PluginEditor() override;

    void paint(juce::Graphics &) override;

    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)

    PluginProcessor &getProcessor();

    [[nodiscard]] const PluginProcessor &getProcessor() const;

    ananas::UI::AnanasLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;
    ananas::UI::NetworkOverviewComponent networkOverview;
};


#endif //PLUGINEDITOR_H
