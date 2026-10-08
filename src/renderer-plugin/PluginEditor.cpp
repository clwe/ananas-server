#include "PluginEditor.h"
#include <AnanasUtils.h>
#include "WFSUtils.h"

PluginEditor::PluginEditor(PluginProcessor &p)
    : AudioProcessorEditor(&p),
      tooltipWindow(this, ananas::UI::Timing::TooltipDelayTimeMs),
      networkOverview(
          getProcessor().getDynamicTree(),
          getProcessor().getPersistentTree(),
          *getProcessor().getServer().getSwitches()
      ),
      wfsInterface(
          ananas::WFS::Constants::NumSources,
          getProcessor().getParamState(),
          getProcessor().getPersistentTree(),
          getProcessor().getDynamicTree(),
          getProcessor().getSourceAmplitudes(),
          [this](const int slot, const juce::String &moduleIP)
          {
              getProcessor().assignModuleToSlot(slot, moduleIP);
          }
      )
{
    juce::Desktop::getInstance().setDefaultLookAndFeel(&lookAndFeel);

    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(tabbedComponent);
    tabbedComponent.addTab(
        ananas::WFS::Strings::WfsTabName,
        juce::Colours::whitesmoke,
        &wfsInterface,
        false
    );
    tabbedComponent.addTab(
        ananas::WFS::Strings::NetworkTabName,
        juce::Colours::whitesmoke,
        &networkOverview,
        false
    );
    lookAndFeel.setNumberOfTabs(tabbedComponent.getNumTabs());

    setSize(ananas::UI::Dimensions::UiWidth, ananas::UI::Dimensions::UiHeight);

    getProcessor().getPersistentTree().addListener(this);
    getProcessor().getDynamicTree().addListener(this);

#if SHOW_NO_NETWORK_OVERLAY
    getProcessor().getServer().addChangeListener(&wfsInterface);
    getProcessor().getServer().addChangeListener(&networkOverview);
    getProcessor().getServer().sendChangeMessage();
#endif

    commandManager.setFirstCommandTarget(this);
    commandManager.registerAllCommandsForTarget(this);
    addKeyListener(commandManager.getKeyMappings());
    setWantsKeyboardFocus(true);
}

PluginEditor::~PluginEditor()
{
    juce::Desktop::getInstance().setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);

    getProcessor().getPersistentTree().removeListener(this);
    getProcessor().getDynamicTree().removeListener(this);

#if SHOW_NO_NETWORK_OVERLAY
    getProcessor().getServer().removeChangeListener(&wfsInterface);
    getProcessor().getServer().removeChangeListener(&networkOverview);
#endif
}

void PluginEditor::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    lookAndFeel.setTotalWidth(getWidth());
    tabbedComponent.setBounds(getLocalBounds());
}

void PluginEditor::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property)
{
    if (property == ananas::Utils::Identifiers::ClientsShouldRebootParamID) {
        getProcessor().getServer().getClientList()->setShouldReboot(treeWhosePropertyHasChanged[property]);
        treeWhosePropertyHasChanged.setPropertyExcludingListener(this, property, false, nullptr);
    }
}

juce::ApplicationCommandTarget *PluginEditor::getNextCommandTarget()
{
    return nullptr;
}

void PluginEditor::getAllCommands(juce::Array<int> &commands)
{
    commands.addArray({
        ananas::WFS::SwitchToWfsTab,
        ananas::WFS::SwitchToNetworkTab,
        ananas::WFS::ToggleModuleSelectorDisplay
    });

    for (size_t m{0}; m < 10; m++) {
        commands.add(ananas::WFS::RevealModuleSelector + static_cast<int>(m));
    }
}

void PluginEditor::getCommandInfo(const juce::CommandID commandID, juce::ApplicationCommandInfo &result)
{
    switch (commandID) {
        case ananas::WFS::SwitchToWfsTab:
            result.setInfo(
                ananas::WFS::Strings::SwitchToWfsTabShortName,
                ananas::WFS::Strings::SwitchToWfsTabDescription,
                ananas::WFS::Strings::TabsCommandCategoryName,
                0);
            result.addDefaultKeypress(ananas::WFS::UI::Shortcuts::SwitchToWfsTabKeycode, juce::ModifierKeys::noModifiers);
            break;

        case ananas::WFS::SwitchToNetworkTab:
            result.setInfo(
                ananas::WFS::Strings::SwitchToNetworkTabShortName,
                ananas::WFS::Strings::SwitchToNetworkTabDescription,
                ananas::WFS::Strings::TabsCommandCategoryName,
                0);
            result.addDefaultKeypress(ananas::WFS::UI::Shortcuts::SwitchToNetworkTabKeycode, juce::ModifierKeys::noModifiers);
            break;

        case ananas::WFS::ToggleModuleSelectorDisplay:
            result.setInfo(
                ananas::WFS::Strings::ToggleModuleSelectorsShortName,
                ananas::WFS::Strings::ToggleModuleSelectorsDescription,
                ananas::WFS::Strings::OptionsCommandCategoryName,
                wfsInterface.isVisible() ? 0 : juce::ApplicationCommandInfo::CommandFlags::isDisabled);
            result.addDefaultKeypress(ananas::WFS::UI::Shortcuts::ToggleModuleSelectorsKeycode, juce::ModifierKeys::noModifiers);
            break;

        default:
            const auto moduleID{commandID - ananas::WFS::RevealModuleSelector - 1};
            const auto moduleSelectorsAreVisible{
                getProcessor().getParamState().getRawParameterValue(ananas::WFS::Params::ShowModuleSelectors.id)->load() > .5f
            };
            result.setInfo(
                ananas::WFS::Strings::getRevealModuleSelectorShortName(moduleID),
                ananas::WFS::Strings::getRevealModuleSelectorDescription(moduleID),
                ananas::WFS::Strings::ModulesCommandCategoryName,
                wfsInterface.isVisible() && moduleSelectorsAreVisible ? 0 : juce::ApplicationCommandInfo::CommandFlags::isDisabled);
            result.addDefaultKeypress(
                ananas::WFS::UI::Shortcuts::RevealModuleSelectorListKeycode + commandID - ananas::WFS::RevealModuleSelector,
                juce::ModifierKeys::noModifiers
            );
            break;
    }
}

bool PluginEditor::perform(const InvocationInfo &info)
{
    switch (info.commandID) {
        case ananas::WFS::SwitchToWfsTab:
            tabbedComponent.setCurrentTabIndex(0);
            return true;

        case ananas::WFS::SwitchToNetworkTab:
            tabbedComponent.setCurrentTabIndex(1);
            return true;

        case ananas::WFS::ToggleModuleSelectorDisplay: {
            const auto showModuleSelectors{getProcessor().getParamState().getParameter(ananas::WFS::Params::ShowModuleSelectors.id)};
            showModuleSelectors->setValueNotifyingHost(fabsf(showModuleSelectors->getValue() - 1.f));
            return true;
        }

        default:
            wfsInterface.expandModuleList(info.commandID - ananas::WFS::RevealModuleSelector);
            return true;
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
