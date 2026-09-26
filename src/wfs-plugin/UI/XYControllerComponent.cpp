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
        calculateGridSpacingX();

        for (int n{0}; n < numNodesToCreate; ++n) {
            // Add a node for each sound source
            const auto node{nodes.add(new Node{n})};
            addAndMakeVisible(node);
            node->setBroughtToFrontOnMouseClick(true);

            // Add a parameter attachment
            attachments.add(new Attachment{n, apvts, *node});
        }

        startTimerHz(15);
    }

    XYControllerComponent::~XYControllerComponent() = default;

    void XYControllerComponent::paint(juce::Graphics &g)
    {
        if (auto *lnf{dynamic_cast<WfsLookAndFeel *>(&getLookAndFeel())})
            lnf->drawXYController(g, *this);
    }

    void XYControllerComponent::resized()
    {
        for (const auto &node: nodes) {
            node->setBounds();
        }

        calculateGridSpacingX();
    }

    void XYControllerComponent::calculateGridSpacingX()
    {
        // x grid spacing is half the width of the XY controller divided by
        // speaker spacing times the number of modules (i.e. half the number of
        // speakers).
        const auto halfArrayWidth{getSpeakerArrayWidth() / 2};
        const auto halfXYWidth{getWidth() / 2};
        xGridSpacing = halfArrayWidth > 0.f ? static_cast<int>(static_cast<float>(halfXYWidth) / halfArrayWidth) : 0;
    }

    float XYControllerComponent::getSpeakerArrayWidth() const
    {
        return speakerArrayWidth;
    }

    void XYControllerComponent::setArrayGeometry(const float arrayWidthMetres, const int numRenderedSources)
    {
        speakerArrayWidth = arrayWidthMetres;
        calculateGridSpacingX();

        for (const auto &node: nodes) {
            node->setDimmed(node->getIndex() >= numRenderedSources);
            node->updateTooltip();
        }

        repaint();
    }

    void XYControllerComponent::mouseDown(const juce::MouseEvent &event)
    {
        if (event.mods.isPopupMenu()) {
            // Show a menu to display a hidden node if any are indeed hidden.
            juce::PopupMenu m;
            auto hasInvisibleNodes{false};
            for (int n{0}; n < nodes.size(); ++n) {
                if (!nodes[n]->isVisible()) {
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
                            node->setVisible(true);
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

    void XYControllerComponent::hideAllNodesBesides(const int nodeIdNotToHide)
    {
        for (const auto &node: nodes) {
            if (node->getIndex() != nodeIdNotToHide) {
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

    int XYControllerComponent::getGridSpacingX() const
    {
        return xGridSpacing;
    }

    int XYControllerComponent::getMaxYMetres() const
    {
        return Constants::MaxYMetres;
    }

    int XYControllerComponent::getMinYMetres() const
    {
        return Constants::MinYMetres;
    }

    //==========================================================================

    XYControllerComponent::Node::Node(const int idx) : index(idx)
    {
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

        const auto parent{getParentComponent()};
        const auto parentBounds{parent->getBounds().toFloat()};
        const auto origin{
            juce::Point{
                parentBounds.getWidth() / 2,
                Constants::MaxYMetres * parentBounds.getHeight() /
                (Constants::MaxYMetres - Constants::MinYMetres)
            }
        };
        auto mousePos{event.getEventRelativeTo(parent).position};

        mousePos -= origin;

        auto newVal{mousePos};

        // Normalise x to [-1, 1]
        newVal.x /= (parentBounds.getWidth() / 2.f);

        // Normalize y to [-1, 1]
        if (mousePos.y <= 0) {
            // Above zero: map [-yZero, 0] to [1, 0]
            newVal.y = -mousePos.y / origin.y;
        } else {
            // Below zero: map [0, height-yZero] to [0, -1]
            newVal.y = -mousePos.y / (parentBounds.getHeight() - origin.y);
        }

        // Set node value.
        setValueX(juce::jlimit(
                      Params::VirtualSourcePositionRange.start,
                      Params::VirtualSourcePositionRange.end,
                      newVal.x),
                  juce::sendNotificationSync);
        setValueY(juce::jlimit(
                      Params::VirtualSourcePositionRange.start,
                      Params::VirtualSourcePositionRange.end,
                      newVal.y),
                  juce::sendNotificationSync);

        setBounds();
    }

    void XYControllerComponent::Node::mouseUp(const juce::MouseEvent &event)
    {
        ignoreUnused(event);
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


    void XYControllerComponent::Node::setBounds()
    {
        const auto parentBounds{getParentComponent()->getBounds().toFloat()};
        const auto origin{
            juce::Point{
                parentBounds.getWidth() / 2,
                Constants::MaxYMetres * parentBounds.getHeight() /
                (Constants::MaxYMetres - Constants::MinYMetres)
            }
        };

        const auto x{(value.x + 1.f) * origin.x};
        auto y{0.f};

        if (value.y >= 0) {
            y = origin.y - origin.y * value.y;
        } else {
            y = origin.y - (parentBounds.getHeight() - origin.y) * value.y;
        }

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
        const auto w{xy->getSpeakerArrayWidth()};
        setTooltip(Utils::normalisedPointToCoordinateMetres(
                       value,
                       juce::Point{
                           -w / 2,
                           static_cast<float>(Constants::MinYMetres)
                       },
                       juce::Point{
                           w / 2,
                           static_cast<float>(Constants::MaxYMetres)
                       }) +
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
        const uint sourceIndex,
        const juce::AudioProcessorValueTreeState &state,
        Node &n,
        juce::UndoManager *um
    ) : node(n),
        attachmentX(*state.getParameter(Params::getVirtualSourcePositionParamID(sourceIndex, SourcePositionAxis::X)), [this](const float f) { setValueX(f); }, um),
        attachmentY(*state.getParameter(Params::getVirtualSourcePositionParamID(sourceIndex, SourcePositionAxis::Y)), [this](const float f) { setValueY(f); }, um)
    {
        node.addListener(this);
        attachmentX.sendInitialUpdate();
        attachmentY.sendInitialUpdate();
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

    void XYControllerComponent::ParameterAttachment::valueChanged(Node *)
    {
        if (!ignoreCallbacks) {
            const auto val{node.getValue()};
            attachmentX.setValueAsPartOfGesture(val.x);
            attachmentY.setValueAsPartOfGesture(val.y);
        }
    }

    void XYControllerComponent::ParameterAttachment::dragStarted(Node *)
    {
        attachmentX.beginGesture();
        attachmentY.beginGesture();
    }

    void XYControllerComponent::ParameterAttachment::dragEnded(Node *)
    {
        attachmentX.endGesture();
        attachmentY.endGesture();
    }

    //==========================================================================

    XYControllerComponent::Attachment::Attachment(
        const int sourceIndex,
        juce::AudioProcessorValueTreeState &state,
        Node &node
    ) : attachment(std::make_unique<ParameterAttachment>(sourceIndex, state, node, state.undoManager))
    {
    }
}
