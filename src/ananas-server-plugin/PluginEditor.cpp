#include "PluginEditor.h"
#include <AnanasUIUtils.h>
#include <AnanasUtils.h>

PluginEditor::PluginEditor(PluginProcessor &p)
    : AudioProcessorEditor(&p),
      tooltipWindow(this, ananas::UI::Timing::TooltipDelayTimeMs),
      networkOverview(
          getProcessor().getDynamicTree(),
          getProcessor().getPersistentTree(),
          *getProcessor().getServer().getSwitches()
      )
{
    juce::Desktop::getInstance().setDefaultLookAndFeel(&lookAndFeel);

    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(networkOverview);

    setSize(ananas::UI::Dimensions::UiWidth, ananas::UI::Dimensions::UiHeight);

    getProcessor().getPersistentTree().addListener(this);
    getProcessor().getDynamicTree().addListener(this);

#if SHOW_NO_NETWORK_OVERLAY
    getProcessor().getServer().addChangeListener(&networkOverview);
    getProcessor().getServer().sendChangeMessage();
#endif
}

PluginEditor::~PluginEditor()
{
    juce::Desktop::getInstance().setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);

    getProcessor().getPersistentTree().removeListener(this);
    getProcessor().getDynamicTree().removeListener(this);

#if SHOW_NO_NETWORK_OVERLAY
    getProcessor().getServer().removeChangeListener(&networkOverview);
#endif
}

void PluginEditor::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    networkOverview.setBounds(getLocalBounds());
}

void PluginEditor::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property)
{
    if (property == ananas::Utils::Identifiers::ClientsShouldRebootParamID) {
        getProcessor().getServer().getClientList()->setShouldReboot(treeWhosePropertyHasChanged[property]);
        treeWhosePropertyHasChanged.setPropertyExcludingListener(this, property, false, nullptr);
    }
}

PluginProcessor &PluginEditor::getProcessor()
{
    return dynamic_cast<PluginProcessor &>(processor);
}

const PluginProcessor &PluginEditor::getProcessor() const
{
    return dynamic_cast<const PluginProcessor &>(processor);
}
