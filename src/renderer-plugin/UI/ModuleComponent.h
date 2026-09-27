#ifndef MODULESELECTORCOMPONENT_H
#define MODULESELECTORCOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>

namespace ananas::WFS::UI
{
    /**
     * Selects which module sits in one slot of the array.
     */
    class ModuleComponent final : public juce::Component
    {
    public:
        using SelectionCallback = std::function<void(int slot, const juce::String &moduleIP)>;

        ModuleComponent(int slotIndex, SelectionCallback onModuleSelected);

        void resized() override;

        /**
         * @param ips Connected modules to choose from.
         * @param selectedIP The module currently in this slot, if any.
         */
        void setAvailableModules(const juce::StringArray &ips, const juce::String &selectedIP);

        void shouldShowModuleSelector(bool show);

        void expandModuleList();

        void collapseModuleList();

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleComponent)

        static constexpr int NoModuleItemID{1};
        static constexpr int FirstModuleItemID{2};

        int slot;
        SelectionCallback onModuleSelected;
        bool showModuleSelector{false};
        juce::ComboBox moduleSelector;
    };
} // ananas::WFS

#endif //MODULESELECTORCOMPONENT_H
