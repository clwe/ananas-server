#ifndef SPEAKERTABLECOMPONENT_H
#define SPEAKERTABLECOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <AnanasNetworkTable.h>

namespace ananas::WFS::UI
{
    /**
     * Read-only table of the speakers the renderers use, with the problems
     * checkLayout() found. Shows the processor's SpeakerLayout node in the
     * dynamic tree.
     */
    class SpeakerTableComponent final : public juce::Component,
                                        public juce::ValueTree::Listener,
                                        juce::AsyncUpdater
    {
    public:
        explicit SpeakerTableComponent(juce::ValueTree &dynamicTree);

        ~SpeakerTableComponent() override;

        void resized() override;

        void valueTreePropertyChanged(juce::ValueTree &tree, const juce::Identifier &property) override;

        void valueTreeChildAdded(juce::ValueTree &parent, juce::ValueTree &child) override;

        void valueTreeChildRemoved(juce::ValueTree &parent, juce::ValueTree &child, int index) override;

        void handleAsyncUpdate() override;

    private:
        class Table final : public ::ananas::UI::AnanasNetworkTable
        {
        public:
            struct Row
            {
                int index = 0;
                float x = 0.f;
                float y = 0.f;
                juce::String moduleIp;
                int output = -1;
                // Worst LayoutProblem::Severity, as an int; -1 if none.
                int severity = -1;
                juce::StringArray problems;
            };

            Table();

            void setRows(std::vector<Row> newRows);

            int getNumRows() override;

            void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                    bool rowIsSelected) override;

            void paintCell(juce::Graphics &g, int rowNumber, int columnId, int width, int height,
                           bool rowIsSelected) override;

            juce::String getCellTooltip(int rowNumber, int columnId) override;

            void resized() override;

        private:
            std::vector<Row> rows;
        };

        void update();

        juce::ValueTree speakerLayoutNode;
        juce::Label title;
        Table table;
        // Problems that concern a module rather than a speaker.
        juce::Label moduleProblemsLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpeakerTableComponent)
    };
}

#endif //SPEAKERTABLECOMPONENT_H
