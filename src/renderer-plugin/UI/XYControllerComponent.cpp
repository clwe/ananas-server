#include "XYControllerComponent.h"

#include "WfsLookAndFeel.h"
#include "../WFSUtils.h"

namespace ananas::WFS::UI
{
    XYControllerComponent::XYControllerComponent(
        const int numNodesToCreate,
        juce::AudioProcessorValueTreeState &apvts,
        juce::HashMap<int, std::atomic<float> *> &sourceAmplitudes
    ) : nodeIntensities(sourceAmplitudes)
    {
        // The speakers underneath; clicks go to this component (for the
        // menu) and to the nodes.
        addAndMakeVisible(layoutView);
        layoutView.setInterceptsMouseClicks(false, false);
        layoutView.onTransformChanged = [this]
        {
            updateNodeBounds();
            if (onViewChanged) onViewChanged();
        };

        zoomInButton.setButtonText("+");
        zoomInButton.setTooltip("Zoom in (or use the mouse wheel)");
        zoomInButton.onClick = [this] { setZoom(layoutView.getZoom() * 1.5f); };
        zoomOutButton.setButtonText("-");
        zoomOutButton.setTooltip("Zoom out (or use the mouse wheel)");
        zoomOutButton.onClick = [this] { setZoom(layoutView.getZoom() / 1.5f); };
        zoomFitButton.setButtonText("Fit");
        zoomFitButton.setTooltip("Fit the speakers");
        zoomFitButton.onClick = [this] { setZoom(1.f); };
        for (auto *b: {&zoomInButton, &zoomOutButton, &zoomFitButton}) {
            addAndMakeVisible(b);
        }

        for (int n{0}; n < numNodesToCreate; ++n) {
            // Add a node for each sound source
            const auto node{nodes.add(new Node{n})};
            addAndMakeVisible(node);
            node->setBroughtToFrontOnMouseClick(true);

            // Add a parameter attachment
            const auto index{static_cast<uint>(n)};
            attachments.add(new Attachment{
                Params::getVirtualSourcePositionParamID(index, SourcePositionAxis::X),
                Params::getVirtualSourcePositionParamID(index, SourcePositionAxis::Y),
                Params::getVirtualSourcePositionParamID(index, SourcePositionAxis::Z),
                apvts, *node
            });
        }

        // The Ambisonics listening point, shown while Ambisonics modules are
        // connected.
        listenerNode = nodes.add(new Node{Node::ListenerIndex});
        addChildComponent(listenerNode);
        listenerNode->setBroughtToFrontOnMouseClick(true);
        listenerNode->setColour(Node::backgroundColourId, juce::Colours::orange.withAlpha(.6f));
        listenerNode->setColour(Node::borderColourId, juce::Colours::darkorange);
        attachments.add(new Attachment{Params::ListenerX.id, Params::ListenerY.id, {}, apvts, *listenerNode});

        startTimerHz(15);
    }

    XYControllerComponent::~XYControllerComponent() = default;

    void XYControllerComponent::paint(juce::Graphics &g)
    {
        // The layout view fills the component.
        juce::ignoreUnused(g);
    }

    void XYControllerComponent::resized()
    {
        layoutView.setBounds(getLocalBounds());

        auto buttons = getLocalBounds().reduced(6).removeFromTop(24).removeFromRight(3 * 34);
        zoomOutButton.setBounds(buttons.removeFromLeft(30));
        buttons.removeFromLeft(2);
        zoomInButton.setBounds(buttons.removeFromLeft(30));
        buttons.removeFromLeft(2);
        zoomFitButton.setBounds(buttons);

        updateNodeBounds();
    }

    void XYControllerComponent::updateNodeBounds()
    {
        for (const auto &node: nodes) {
            node->setBounds();
        }
    }

    void XYControllerComponent::setZoom(const float factor)
    {
        layoutView.setZoom(factor);
    }

    void XYControllerComponent::setArrayGeometry(const float outerSpeakerXMetres, const int numRenderedSources)
    {
        outerSpeakerX = outerSpeakerXMetres;

        // Where sources can be: x scaled to the outer speakers, y to its range.
        layoutView.setExtraArea(juce::Rectangle<float>{
            juce::Point<float>{-outerSpeakerX, static_cast<float>(Constants::MinYMetres)},
            juce::Point<float>{outerSpeakerX, static_cast<float>(Constants::MaxYMetres)}
        });

        for (const auto &node: nodes) {
            node->setDimmed(node->getIndex() >= numRenderedSources);
            node->updateTooltip();
        }

        updateNodeBounds();
        repaint();
    }

    void XYControllerComponent::setSpeakers(std::vector<LayoutView::ViewSpeaker> speakers)
    {
        layoutView.setSpeakers(std::move(speakers));
    }

    juce::Point<float> XYControllerComponent::metresToView(const juce::Point<float> metres) const
    {
        return layoutView.toView(metres);
    }

    juce::Point<float> XYControllerComponent::valueToView(const juce::Point<float> value) const
    {
        return metresToView({Params::positionXToMetres(value.x, outerSpeakerX), Params::positionYToMetres(value.y)});
    }

    juce::Point<float> XYControllerComponent::viewToValue(const juce::Point<float> view) const
    {
        const auto metres = layoutView.toMetres(view);
        const auto x = outerSpeakerX > 0.f ? metres.x / outerSpeakerX : 0.f;
        const auto y = metres.y < 0.f ? metres.y / -static_cast<float>(Constants::MinYMetres)
                                      : metres.y / static_cast<float>(Constants::MaxYMetres);
        const auto &range = Params::VirtualSourcePositionRange;
        return {juce::jlimit(range.start, range.end, x), juce::jlimit(range.start, range.end, y)};
    }

    float XYControllerComponent::getScale() const
    {
        return layoutView.getScale();
    }

    float XYControllerComponent::getOuterSpeakerX() const
    {
        return outerSpeakerX;
    }

    void XYControllerComponent::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
    {
        juce::ignoreUnused(event);
        setZoom(layoutView.getZoom() * std::pow(1.5f, wheel.deltaY * 4.f));
    }

    void XYControllerComponent::mouseDown(const juce::MouseEvent &event)
    {
        if (event.mods.isPopupMenu()) {
            // Show a menu to display a hidden node if any are indeed hidden.
            juce::PopupMenu m;
            auto hasInvisibleNodes{false};
            for (int n{0}; n < nodes.size(); ++n) {
                if (!nodes[n]->isListener() && !nodes[n]->isVisible()) {
                    if (!hasInvisibleNodes) {
                        m.addSectionHeader(MenuItems::ShowSourceHeader);
                        m.addItem(MenuItems::ShowAllSources.id, MenuItems::ShowAllSources.text);
                    }
                    hasInvisibleNodes = true;
                    const auto nodeID{static_cast<int>(nodes[n]->getIndex() + 1)};
                    m.addItem(nodeID, juce::String{nodeID});
                }
            }

            if (m.containsAnyActiveItems()) {
                m.showMenuAsync(juce::PopupMenu::Options(), [this](const int result)
                {
                    if (result == MenuItems::ShowAllSourcesMenuID) {
                        for (const auto &node: nodes) {
                            if (!node->isListener()) node->setVisible(true);
                        }
                        return;
                    }

                    for (const auto &node: nodes) {
                        if (node->getIndex() == result - 1) {
                            node->setVisible(true);
                        }
                    }
                });
            }
        }
    }

    void XYControllerComponent::setListenerVisible(const bool shouldBeVisible)
    {
        listenerNode->setVisible(shouldBeVisible);
        if (shouldBeVisible) {
            listenerNode->setBounds();
            listenerNode->toFront(false);
        }
    }

    void XYControllerComponent::hideAllNodesBesides(const int nodeIdNotToHide)
    {
        for (const auto &node: nodes) {
            if (!node->isListener() && node->getIndex() != nodeIdNotToHide) {
                node->hide();
            }
        }
    }

    void XYControllerComponent::timerCallback()
    {
        for (auto n{0}; n < nodeIntensities.size(); ++n) {
            nodes[n]->setIntensity(nodeIntensities[n]->load());
        }
    }


    //==========================================================================

    XYControllerComponent::Node::Node(const int idx) : index(idx)
    {
    }

    bool XYControllerComponent::Node::isListener() const
    {
        return index == ListenerIndex;
    }

    juce::String XYControllerComponent::Node::getLabel() const
    {
        return isListener() ? juce::String{"L"} : juce::String{index + 1};
    }

    void XYControllerComponent::Node::paint(juce::Graphics &g)
    {
        if (const auto *lnf{dynamic_cast<WfsLookAndFeel *>(&getLookAndFeel())}) {
            if (dimmed) g.beginTransparencyLayer(.3f);
            lnf->drawXYControllerNode(g, *this, intensity);
            if (dimmed) g.endTransparencyLayer();
        }
    }

    void XYControllerComponent::Node::mouseDown(const juce::MouseEvent &event)
    {
        if (event.mods.isPopupMenu() && event.originalComponent == this) {
            // Only sources can be hidden.
            if (isListener()) return;

            juce::PopupMenu m;
            m.addItem(MenuItems::HideSource.id, MenuItems::HideSource.text);
            m.addItem(MenuItems::HideOtherSources.id, MenuItems::HideOtherSources.text);
            m.showMenuAsync(juce::PopupMenu::Options(), [this](const int result)
            {
                switch (result) {
                    case MenuItems::HideSourceMenuID:
                        hide();
                        break;
                    case MenuItems::HideOtherSourcesMenuID:
                        dynamic_cast<XYControllerComponent *>(getParentComponent())->hideAllNodesBesides(index);
                        break;
                    default:
                        break;
                }
            });
        } else {
            // Alt-drag changes a source's height instead of its position.
            heightDragStart.reset();
            if (event.mods.isAltDown() && !isListener()) {
                heightDragStart = height;
            }
            currentDrag.reset();
            currentDrag = std::make_unique<ScopedDragNotification>(*this);
        }
    }

    void XYControllerComponent::Node::hide()
    {
        setVisible(false);
    }

    void XYControllerComponent::Node::mouseDrag(const juce::MouseEvent &event)
    {
        if (!event.mods.isLeftButtonDown()) return;

        const auto *xy{dynamic_cast<XYControllerComponent *>(getParentComponent())};
        if (xy == nullptr) return;

        if (heightDragStart) {
            // To scale: dragging up by one metre of the grid raises the source
            // by one metre.
            const auto metres{-static_cast<float>(event.getDistanceFromDragStartY()) / juce::jmax(xy->getScale(), 1e-3f)};
            setHeight(Params::VirtualSourceZRange.snapToLegalValue(*heightDragStart + metres), juce::sendNotificationSync);
            updateTooltip();
            repaint();
            return;
        }

        const auto newVal{xy->viewToValue(event.getEventRelativeTo(getParentComponent()).position)};
        setValueX(newVal.x, juce::sendNotificationSync);
        setValueY(newVal.y, juce::sendNotificationSync);

        setBounds();
    }

    void XYControllerComponent::Node::mouseUp(const juce::MouseEvent &event)
    {
        ignoreUnused(event);
        heightDragStart.reset();
        currentDrag.reset();
        updateTooltip();
    }

    void XYControllerComponent::Node::setValueX(const float newX, const juce::NotificationType notification)
    {
        value.x = newX;
        triggerChangeMessage(notification);
    }


    void XYControllerComponent::Node::setValueY(const float newY, const juce::NotificationType notification)
    {
        value.y = newY;
        triggerChangeMessage(notification);
    }


    void XYControllerComponent::Node::setHeight(const float newHeight, const juce::NotificationType notification)
    {
        if (juce::exactlyEqual(newHeight, height)) return;
        height = newHeight;
        repaint();
        triggerChangeMessage(notification);
    }

    float XYControllerComponent::Node::getHeight() const
    {
        return height;
    }

    void XYControllerComponent::Node::setBounds()
    {
        const auto *xy{dynamic_cast<XYControllerComponent *>(getParentComponent())};
        if (xy == nullptr) return;

        // Outside the parent, the node simply isn't drawn (children are clipped).
        const auto centre{xy->valueToView(value)};
        const auto x{centre.x};
        const auto y{centre.y};

        Component::setBounds(
            static_cast<int>(x - Dimensions::NodeHalfDiameter),
            static_cast<int>(y - Dimensions::NodeHalfDiameter),
            Dimensions::NodeDiameter,
            Dimensions::NodeDiameter
        );
    }

    void XYControllerComponent::Node::handleAsyncUpdate()
    {
        cancelPendingUpdate();

        const BailOutChecker checker{this};
        listeners.callChecked(checker, [&](Listener &l)
        {
            l.valueChanged(this);
        });

        if (checker.shouldBailOut()) return;

        if (const auto *handler{getAccessibilityHandler()}) {
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        }
    }

    juce::Point<float> XYControllerComponent::Node::getValue() const
    {
        return value;
    }

    void XYControllerComponent::Node::sendDragStart()
    {
        const BailOutChecker checker{this};
        listeners.callChecked(checker, [&](Listener &l) { l.dragStarted(this); });
    }

    void XYControllerComponent::Node::sendDragEnd()
    {
        const BailOutChecker checker{this};
        listeners.callChecked(checker, [&](Listener &l) { l.dragEnded(this); });
    }

    void XYControllerComponent::Node::updateTooltip()
    {
        const XYControllerComponent *xy{dynamic_cast<XYControllerComponent *>(getParentComponent())};
        if (xy == nullptr) return;
        // The same conversion as the renderers (x scaled to the outer speakers).
        const auto outerX{xy->getOuterSpeakerX()};
        setTooltip((isListener() ? juce::String{"Listener "} : juce::String{}) +
                   Utils::normalisedPointToCoordinateMetres(
                       value,
                       juce::Point{
                           -outerX,
                           static_cast<float>(Constants::MinYMetres)
                       },
                       juce::Point{
                           outerX,
                           static_cast<float>(Constants::MaxYMetres)
                       }) +
                   (isListener() ? juce::String{} : ", height " + juce::String{height, 2} + " m (Alt-drag to change)") +
                   (dimmed ? juce::String{"\nNot rendered by all modules"} : juce::String{}));
    }

    void XYControllerComponent::Node::addListener(Listener *listener)
    {
        listeners.add(listener);
    }

    void XYControllerComponent::Node::removeListener(Listener *listener)
    {
        listeners.remove(listener);
    }

    int XYControllerComponent::Node::getIndex() const
    {
        return index;
    }

    void XYControllerComponent::Node::setIntensity(const float newIntensity)
    {
        // Avoid unnecessary repaints
        if (isVisible() && std::abs(newIntensity - intensity) > 0.01f) {
            intensity = newIntensity;
            repaint();
        }
    }

    void XYControllerComponent::Node::setDimmed(const bool shouldBeDimmed)
    {
        if (shouldBeDimmed != dimmed) {
            dimmed = shouldBeDimmed;
            repaint();
        }
    }

    void XYControllerComponent::Node::triggerChangeMessage(const juce::NotificationType notification)
    {
        if (notification == juce::dontSendNotification) return;

        if (notification == juce::sendNotificationSync)
            handleAsyncUpdate();
        else
            triggerAsyncUpdate();
    }

    //==========================================================================

    XYControllerComponent::Node::ScopedDragNotification::ScopedDragNotification(Node &node)
        : nodeBeingDragged(node)
    {
        nodeBeingDragged.sendDragStart();
    }

    XYControllerComponent::Node::ScopedDragNotification::~ScopedDragNotification()
    {
        nodeBeingDragged.sendDragEnd();
    }

    //==========================================================================

    XYControllerComponent::ParameterAttachment::ParameterAttachment(
        const juce::String &paramIdX,
        const juce::String &paramIdY,
        const juce::String &paramIdZ,
        const juce::AudioProcessorValueTreeState &state,
        Node &n,
        juce::UndoManager *um
    ) : node(n),
        attachmentX(*state.getParameter(paramIdX), [this](const float f) { setValueX(f); }, um),
        attachmentY(*state.getParameter(paramIdY), [this](const float f) { setValueY(f); }, um)
    {
        if (paramIdZ.isNotEmpty()) {
            attachmentZ = std::make_unique<juce::ParameterAttachment>(
                *state.getParameter(paramIdZ), [this](const float f) { setHeight(f); }, um);
        }

        node.addListener(this);
        attachmentX.sendInitialUpdate();
        attachmentY.sendInitialUpdate();
        if (attachmentZ) attachmentZ->sendInitialUpdate();
        node.updateTooltip();
    }

    XYControllerComponent::ParameterAttachment::~ParameterAttachment()
    {
        node.removeListener(this);
    }

    void XYControllerComponent::ParameterAttachment::setValueX(const float newX)
    {
        const juce::ScopedValueSetter svs(ignoreCallbacks, true);
        node.setValueX(newX, juce::sendNotificationSync);
        node.setBounds();
    }

    void XYControllerComponent::ParameterAttachment::setValueY(const float newY)
    {
        const juce::ScopedValueSetter svs(ignoreCallbacks, true);
        node.setValueY(newY, juce::sendNotificationSync);
        node.setBounds();
    }

    void XYControllerComponent::ParameterAttachment::setHeight(const float newHeight)
    {
        const juce::ScopedValueSetter svs(ignoreCallbacks, true);
        node.setHeight(newHeight, juce::sendNotificationSync);
        node.updateTooltip();
    }

    void XYControllerComponent::ParameterAttachment::valueChanged(Node *)
    {
        if (!ignoreCallbacks) {
            const auto val{node.getValue()};
            attachmentX.setValueAsPartOfGesture(val.x);
            attachmentY.setValueAsPartOfGesture(val.y);
            if (attachmentZ) attachmentZ->setValueAsPartOfGesture(node.getHeight());
        }
    }

    void XYControllerComponent::ParameterAttachment::dragStarted(Node *)
    {
        attachmentX.beginGesture();
        attachmentY.beginGesture();
        if (attachmentZ) attachmentZ->beginGesture();
    }

    void XYControllerComponent::ParameterAttachment::dragEnded(Node *)
    {
        attachmentX.endGesture();
        attachmentY.endGesture();
        if (attachmentZ) attachmentZ->endGesture();
    }

    //==========================================================================

    XYControllerComponent::Attachment::Attachment(
        const juce::String &paramIdX,
        const juce::String &paramIdY,
        const juce::String &paramIdZ,
        juce::AudioProcessorValueTreeState &state,
        Node &node
    ) : attachment(std::make_unique<ParameterAttachment>(paramIdX, paramIdY, paramIdZ, state, node, state.undoManager))
    {
    }
}
