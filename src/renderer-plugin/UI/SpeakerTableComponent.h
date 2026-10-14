#ifndef SPEAKERTABLECOMPONENT_H
#define SPEAKERTABLECOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <AnanasNetworkTable.h>
#include "../CustomLayout.h"
#include "../LayoutFile.h"

namespace ananas::WFS::UI
{
    /**
     * Table of the speakers the renderers use, with the problems
     * checkLayout() found, and the choice between the linear array and the
     * custom layout.
     *
     * Shows the processor's SpeakerLayout node in the dynamic tree; reads the
     * layout mode and the modules from the persistent tree. With the custom
     * layout, speakers can be edited, added and removed; edits go straight to
     * the CustomLayout, and come back through the trees.
     */
    class SpeakerTableComponent final : public juce::Component,
                                        public juce::ValueTree::Listener,
                                        juce::AsyncUpdater
    {
    public:
        SpeakerTableComponent(juce::ValueTree &dynamicTree,
                              juce::ValueTree &persistentTree,
                              CustomLayout &customLayout,
                              std::function<void(LayoutMode)> onLayoutModeSelected,
                              std::function<void()> onConvertToCustomLayout,
                              std::function<const SpeakerLayout &()> getLayoutToSave,
                              std::function<void(std::vector<Speaker>)> onLayoutLoaded,
                              std::function<juce::Point<float>()> getListenerPosition);

        ~SpeakerTableComponent() override;

        void resized() override;

        void valueTreePropertyChanged(juce::ValueTree &tree, const juce::Identifier &property) override;

        void valueTreeChildAdded(juce::ValueTree &parent, juce::ValueTree &child) override;

        void valueTreeChildRemoved(juce::ValueTree &parent, juce::ValueTree &child, int index) override;

        void handleAsyncUpdate() override;

    private:
        // A module the user can assign speakers to.
        struct ModuleOption
        {
            juce::String ip;
            juce::String label;
            int numOutputs = 0;
        };

        class Table final : public ::ananas::UI::AnanasNetworkTable
        {
        public:
            struct Row
            {
                int index = 0;
                float x = 0.f;
                float y = 0.f;
                float z = 0.f;
                Direction facing;
                juce::String moduleIp;
                int output = -1;
                // Worst LayoutProblem::Severity, as an int; -1 if none.
                int severity = -1;
                juce::StringArray problems;
            };

            Table();

            void setRows(std::vector<Row> newRows, std::vector<ModuleOption> newModules, bool isEditable);

            [[nodiscard]] juce::SparseSet<int> getSelectedRows() const;

            int getNumRows() override;

            void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                    bool rowIsSelected) override;

            void paintCell(juce::Graphics &g, int rowNumber, int columnId, int width, int height,
                           bool rowIsSelected) override;

            juce::Component *refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                                     juce::Component *existingComponentToUpdate) override;

            juce::String getCellTooltip(int rowNumber, int columnId) override;

            void resized() override;

            // Edits, by speaker index.
            std::function<void(int index, juce::Point<float> position)> onPositionEdited;
            std::function<void(int index, float z)> onHeightEdited;
            std::function<void(int index, Direction facing)> onFacingEdited;
            std::function<void(int index, const juce::String &moduleIp, int output)> onModuleEdited;

        private:
            class NumberCell;
            class ModuleCell;
            class OutputCell;

            [[nodiscard]] const ModuleOption *findModule(const juce::String &ip) const;

            std::vector<Row> rows;
            std::vector<ModuleOption> modules;
            bool editable = false;
        };

        void update();

        // Runs `replace` right away if the custom layout is empty, otherwise
        // after asking.
        void confirmReplacingCustomLayout(const juce::String &message, std::function<void()> replace);

        void saveLayout();

        // Ananas layout files, and IEM loudspeaker layouts (which first ask
        // for the radius and the centre).
        void loadLayout();

        void askIemPlacement(const juce::String &fileName, const juce::var &json);

        // Shows errors, or asks before replacing the custom layout, then
        // loads the speakers and shows any warnings.
        void applyLoadedLayout(LayoutFile::ReadResult result, const juce::String &fileName);

        CustomLayout &customLayout;
        std::function<void(LayoutMode)> onLayoutModeSelected;
        std::function<void()> onConvertToCustomLayout;
        std::function<const SpeakerLayout &()> getLayoutToSave;
        std::function<void(std::vector<Speaker>)> onLayoutLoaded;
        std::function<juce::Point<float>()> getListenerPosition;
        // Kept while its dialog is open.
        std::unique_ptr<juce::FileChooser> fileChooser;

        juce::ValueTree speakerLayoutNode;
        juce::ValueTree layoutNode;
        juce::ValueTree modulesNode;

        juce::Label title;
        juce::Label layoutModeLabel;
        juce::ComboBox layoutModeSelector;
        juce::TextButton convertButton;
        juce::TextButton saveButton;
        juce::TextButton loadButton;
        juce::TextButton addSpeakerButton;
        juce::TextButton removeSpeakersButton;
        Table table;
        // Problems that concern a module rather than a speaker.
        juce::Label moduleProblemsLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpeakerTableComponent)
    };
}

#endif //SPEAKERTABLECOMPONENT_H
