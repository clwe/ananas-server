#ifndef SWITCHESCOMPONENT_H
#define SWITCHESCOMPONENT_H

#include <AnanasLookAndFeel.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "AnanasNetworkTable.h"
#include <SwitchInfo.h>

namespace ananas::UI
{
    class SwitchesComponent final : public juce::Component,
                                    public juce::ValueTree::Listener,
                                    juce::AsyncUpdater
    {
    public:
        SwitchesComponent(juce::ValueTree &dynamicTreeRef, juce::ValueTree &persistentTreeRef, SwitchList &switchList);

        ~SwitchesComponent() override;

        //======================================================================

        enum ColourIds
        {
            backgroundColourId = 0x2001200
        };

        //======================================================================

        void paint(juce::Graphics &g) override;

        void resized() override;

        void update();

        void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) override;

        void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) override;

        void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) override;

        void handleAsyncUpdate() override;

        void addSwitch() const;

        void removeSwitch(const juce::Identifier &switchID) const;

        void resetPtpForSwitch(const juce::Identifier &switchID) const;

        class SwitchesTable final : public AnanasNetworkTable
        {
        public:
            SwitchesTable();

            void update(const juce::ValueTree& switchesNode);

            int getNumRows() override;

            void paintRowBackground(juce::Graphics &, int rowNumber, int width, int height, bool rowIsSelected) override;

            void paintCell(juce::Graphics &, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;

            void resized() override;

            Component *refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, Component *existingComponentToUpdate) override;

            void handleResetPtpForSwitch(const juce::Identifier& switchID) const;

            void handleRemoveSwitch(const juce::Identifier& switchID) const;

            [[nodiscard]] juce::Identifier getSwitchID(int rowNumber) const;

            std::function<void(int rowNumber, int columnId, juce::String value)> onCellEdited;
            std::function<void(juce::Identifier switchID)> onSwitchRemoved;
            std::function<void(juce::Identifier switchID)> onResetPtpForSwitch;
            bool isEditing{false};

            class RemoveSwitchButton final : public juce::TextButton
            {
            public:
                using TextButton::TextButton;
            };

        private:
            struct Row
            {
                juce::Identifier id;
                juce::String ip;
                juce::String username;
                juce::String password;
                juce::int32 freqDriftPPB{0};
                juce::int32 offsetNS{0};
            };

            inline static const juce::Array<int> editableColumnIDs{1, 2, 3};
            constexpr static int passwordColumnID{3};
            juce::Array<Row> rows;
        };

        void updateSwitch(const juce::Identifier &switchID, int col, const juce::String &content) const;

        juce::Label title;
        juce::TextButton addSwitchButton;
        SwitchesTable switchesTable;
        SwitchList &switchList;
        juce::ValueTree &dynamicTree;
        juce::ValueTree &persistentTree;
        juce::ValueTree switchesNode;
    };
} // ananas::UI

#endif //SWITCHESCOMPONENT_H
