#include "ModuleComponent.h"
#include "WfsLookAndFeel.h"
#include "../WFSUtils.h"

namespace ananas::WFS::UI
{
    ModuleComponent::ModuleComponent(const int slotIndex, SelectionCallback onModuleSelectedCallback)
        : slot(slotIndex),
          onModuleSelected(std::move(onModuleSelectedCallback))
    {
        addAndMakeVisible(moduleSelector);

        moduleSelector.onChange = [this]
        {
            if (onModuleSelected) {
                const auto index{moduleSelector.getSelectedId() - FirstModuleItemID};
                onModuleSelected(slot, juce::isPositiveAndBelow(index, availableIPs.size()) ? availableIPs[index] : juce::String{});
            }
        };
    }

    void ModuleComponent::resized()
    {
        if (showModuleSelector) {
            const auto comboBoxBounds{getLocalBounds().removeFromTop(Dimensions::ModuleSelectorHeight)};
            moduleSelector.setBounds(comboBoxBounds);
            moduleSelector.setVisible(true);
        } else {
            moduleSelector.setVisible(false);
            setBounds(0, 0, 0, 0);
        }
    }

    void ModuleComponent::setAvailableModules(const juce::StringArray &ips, const juce::StringArray &labels, const juce::String &selectedIP)
    {
        availableIPs = ips;
        moduleSelector.clear(juce::dontSendNotification);
        moduleSelector.addItem("-", NoModuleItemID);
        moduleSelector.addItemList(labels, FirstModuleItemID);

        if (selectedIP.isEmpty()) {
            moduleSelector.setSelectedId(NoModuleItemID, juce::dontSendNotification);
        } else if (const auto index{ips.indexOf(selectedIP)}; index >= 0) {
            moduleSelector.setSelectedId(FirstModuleItemID + index, juce::dontSendNotification);
        } else {
            // Assigned, but not currently connected.
            moduleSelector.setText(selectedIP, juce::dontSendNotification);
        }
    }

    void ModuleComponent::shouldShowModuleSelector(const bool show)
    {
        showModuleSelector = show;
        resized();
    }

    void ModuleComponent::expandModuleList()
    {
        moduleSelector.showPopup();
    }

    void ModuleComponent::collapseModuleList()
    {
        moduleSelector.hidePopup();
    }
} // ananas::WFS
