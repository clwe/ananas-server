#ifndef CLIENTSOVERVIEWCOMPONENT_H
#define CLIENTSOVERVIEWCOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "AnanasNetworkTable.h"

namespace ananas::UI
{
    class ClientsOverviewComponent final : public juce::Component,
                                           public juce::ValueTree::Listener,
                                           juce::AsyncUpdater
    {
    public:
        explicit ClientsOverviewComponent(juce::ValueTree &dynamicTree);

        ~ClientsOverviewComponent() override;

        //======================================================================

        enum ColourIds
        {
            backgroundColourId = 0x2001400
        };

        //======================================================================

        void update(const juce::var &var);

        void paint(juce::Graphics &g) override;

        void resized() override;

        void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) override;

        void handleAsyncUpdate() override;

        void triggerClientReboot() const;

        class OverviewPanel final : public Component
        {
        public:
            OverviewPanel();

            juce::String getPresentationTimeIntervalText(int presentationTimeInterval);

            void update(const juce::var &var);

            void paint(juce::Graphics &g) override;

            void resized() override;

            class PresentationTimeInterval final : public juce::Label
            {
            public:
                //==============================================================

                enum ColourIds
                {
                    okColourId = 0x2001410,
                    warningColourId = 0x2001411,
                    oneMicrosecondIndicatorColourId = 0x2001412,
                };

                //==============================================================

                void setBackgroundWidth(float proportion);

                void setMax(float maxSeen);

                void paint(juce::Graphics &g) override;

            private:
                float backgroundProportion{0.f};
                float maxValue{0.f};
            };

            juce::Label totalClientsLabel;
            juce::Label totalClientsValue;
            juce::Label presentationTimeIntervalLabel;
            PresentationTimeInterval presentationTimeIntervalValue;
            juce::int32 maxPresentationTimeInterval{0};
            uint numUpdates{0};
        };

        class ClientTable final : public AnanasNetworkTable
        {
        public:
            ClientTable();

            void update(const juce::var &clientInfo);

            int getNumRows() override;

            void paintRowBackground(juce::Graphics &g, int rowNumber, int width,
                                    int height, bool rowIsSelected) override;

            void paintCell(juce::Graphics &g, int rowNumber, int columnId,
                           int width, int height, bool rowIsSelected) override;

            void resized() override;

        private:
            struct Row
            {
                juce::String ip;
                juce::String serialNumber;
                juce::String firmwareTypeVersion;
                bool ptpLock;
                juce::int32 presentationTimeOffsetNs;
                juce::int32 presentationTimeOffsetFrame;
                juce::int32 audioPTPOffsetNs;
                juce::int32 bufferFillPercent;
                float samplingRate;
                float percentCPU;
                juce::String secondarySourceCoordinates;
                juce::String sourcesSpeakers;
            };

            juce::Array<Row> rows;
        };

    private:
        juce::Label title;
        juce::TextButton rebootAllClientsButton;
        OverviewPanel overviewPanel;
        std::unique_ptr<ClientTable> clientTable;
        juce::ValueTree &tree;
    };
}


#endif //CLIENTSOVERVIEWCOMPONENT_H
