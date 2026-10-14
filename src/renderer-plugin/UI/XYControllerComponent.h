#ifndef XYCONTROLLERCOMPONENT_H
#define XYCONTROLLERCOMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LayoutView.h"

namespace ananas::WFS::UI
{
    /**
     * The sources (and the Ambisonics listener) as draggable nodes over a
     * top-down view of the speakers, to scale and zoomable. Source positions
     * are normalised parameters; they're shown and dragged in metres (x
     * scaled to outerSpeakerX, y to the source y range, as the renderers do).
     * Nodes outside the zoomed view aren't drawn.
     */
    class XYControllerComponent final : public juce::Component,
                                        public juce::Timer
    {
    public:
        XYControllerComponent(int numNodesToCreate,
                              juce::AudioProcessorValueTreeState &apvts,
                              juce::HashMap<int, std::atomic<float> *> &sourceAmplitudes);

        ~XYControllerComponent();

        //======================================================================

        enum ColourIds
        {
            backgroundColourId = 0x2001500,
            gridLineColourId = 0x2001501,
        };

        //======================================================================

        void paint(juce::Graphics &g) override;

        void resized() override;

        /**
         * @param outerSpeakerXMetres Source x is scaled to this (see
         * RenderLayout::outerSpeakerX).
         * @param numRenderedSources Sources rendered by every module in the
         * array; nodes for the others are dimmed.
         */
        void setArrayGeometry(float outerSpeakerXMetres, int numRenderedSources);

        // The speakers drawn underneath the nodes.
        void setSpeakers(std::vector<LayoutView::ViewSpeaker> speakers);

        // Normalised source / listener position to component coordinates,
        // and back (clamped to the parameter range).
        [[nodiscard]] juce::Point<float> valueToView(juce::Point<float> value) const;
        [[nodiscard]] juce::Point<float> viewToValue(juce::Point<float> view) const;

        // Metres (array frame) to component coordinates.
        [[nodiscard]] juce::Point<float> metresToView(juce::Point<float> metres) const;

        // Pixels per metre at the current zoom.
        [[nodiscard]] float getScale() const;

        [[nodiscard]] float getOuterSpeakerX() const;

        // Called when the view's transform changes (size, speakers, zoom).
        std::function<void()> onViewChanged;

        /**
         * Show or hide the Ambisonics listening point.
         */
        void setListenerVisible(bool shouldBeVisible);

        void mouseDown(const juce::MouseEvent &event) override;

        void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

        void hideAllNodesBesides(int nodeIdNotToHide);

        void timerCallback() override;

        class Node final : public Component,
                           public juce::AsyncUpdater,
                           public juce::SettableTooltipClient
        {
        public:
            class Listener;

            /**
             * @param idx Source index; ListenerIndex for the listener.
             */
            explicit Node(int idx);

            static constexpr int ListenerIndex{-1};

            [[nodiscard]] bool isListener() const;

            [[nodiscard]] juce::String getLabel() const;

            //======================================================================

            enum ColourIds
            {
                backgroundColourId = 0x2001600,
                borderColourId = 0x2001601,
                textColourId = 0x2001602,
            };

            //======================================================================

            void paint(juce::Graphics &g) override;

            void mouseDown(const juce::MouseEvent &event) override;

            void hide();

            void mouseDrag(const juce::MouseEvent &event) override;

            void mouseUp(const juce::MouseEvent &event) override;

            void setValueX(float newX, juce::NotificationType notification);

            void setValueY(float newY, juce::NotificationType notification);

            // Height in metres (sources only); changed by Alt-dragging the
            // node up and down.
            void setHeight(float newHeight, juce::NotificationType notification);

            [[nodiscard]] float getHeight() const;

            void setBounds();

            void handleAsyncUpdate() override;

            [[nodiscard]] juce::Point<float> getValue() const;

            void sendDragStart();

            void sendDragEnd();

            void updateTooltip();

            void addListener(Listener *listener);

            void removeListener(Listener *listener);

            int getIndex() const;

            void setIntensity(float newIntensity);

            void setDimmed(bool shouldBeDimmed);

            class Listener
            {
            public:
                virtual ~Listener() = default;

                virtual void valueChanged(Node *) = 0;

                virtual void dragStarted(Node *) = 0;

                virtual void dragEnded(Node *) = 0;
            };

            class ScopedDragNotification
            {
            public:
                explicit ScopedDragNotification(Node &node);

                ~ScopedDragNotification();

            private:
                Node &nodeBeingDragged;

                JUCE_DECLARE_NON_MOVEABLE(ScopedDragNotification)

                JUCE_DECLARE_NON_COPYABLE(ScopedDragNotification)
            };

        private:
            void triggerChangeMessage(juce::NotificationType notification);

            int index{};
            juce::Point<float> value{};
            float height{0.f};
            // While Alt-dragging: the height when the drag started.
            std::optional<float> heightDragStart;
            juce::ListenerList<Listener> listeners;
            std::unique_ptr<ScopedDragNotification> currentDrag;
            float intensity{-100.f};
            bool dimmed{false};
        };

    private:
        class ParameterAttachment final : Node::Listener
        {
        public:
            // paramIdZ empty: no height (the listener).
            ParameterAttachment(const juce::String &paramIdX,
                                const juce::String &paramIdY,
                                const juce::String &paramIdZ,
                                const juce::AudioProcessorValueTreeState &state,
                                Node &n,
                                juce::UndoManager *um = nullptr);

            ~ParameterAttachment() override;

        private:
            void setValueX(float newX);

            void setValueY(float newY);

            void setHeight(float newHeight);

            void valueChanged(Node *node) override;

            void dragStarted(Node *node) override;

            void dragEnded(Node *node) override;

            Node &node;
            juce::ParameterAttachment attachmentX, attachmentY;
            std::unique_ptr<juce::ParameterAttachment> attachmentZ;
            bool ignoreCallbacks{false};
        };

        class Attachment
        {
        public:
            Attachment(const juce::String &paramIdX,
                       const juce::String &paramIdY,
                       const juce::String &paramIdZ,
                       juce::AudioProcessorValueTreeState &state,
                       Node &node);

        private:
            std::unique_ptr<ParameterAttachment> attachment;
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Attachment);
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYControllerComponent)

        void updateNodeBounds();

        void setZoom(float factor);

        // Underneath the nodes; doesn't take mouse clicks.
        LayoutView layoutView;
        // The sources, then the listener.
        juce::OwnedArray<Node> nodes;
        Node *listenerNode{nullptr};
        juce::OwnedArray<Attachment> attachments;
        float outerSpeakerX{1.f};
        juce::TextButton zoomInButton, zoomOutButton, zoomFitButton;
        juce::HashMap<int, std::atomic<float> *> &nodeIntensities;
    };
} // ananas::WFS

#endif //XYCONTROLLERCOMPONENT_H
