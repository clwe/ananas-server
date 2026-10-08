#include "SwitchesComponent.h"
#include <AnanasUtils.h>

namespace ananas::UI
{
    SwitchesComponent::SwitchesComponent(juce::ValueTree &dynamicTreeRef, juce::ValueTree &persistentTreeRef, SwitchList &switchListRef)
        : addSwitchButton(Strings::AddSwitchButtonName),
          switchList(switchListRef),
          dynamicTree(dynamicTreeRef),
          persistentTree(persistentTreeRef)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(addSwitchButton);
        addAndMakeVisible(switchesTable);

        title.setFont(juce::Font{juce::FontOptions{18.f, juce::Font::bold}});
        title.setJustificationType(juce::Justification::centredLeft);
        title.setText(Strings::SwitchesSectionTitle, juce::dontSendNotification);

        addSwitchButton.setButtonText(Strings::AddSwitchButtonText);
        addSwitchButton.setTooltip(Strings::AddSwitchButtonTooltip);
        addSwitchButton.onClick = [this]
        {
            addSwitch();
        };

        dynamicTree.addListener(this);
        persistentTree.addListener(this);

        // The processor mirrors the switch list here.
        switchesNode = persistentTree.getChildWithName(ananas::Utils::Identifiers::SwitchesParamID);

        switchesTable.onCellEdited = [this](const int row, const int col, const juce::String &content)
        {
            updateSwitch(switchesTable.getSwitchID(row), col, content);
        };

        switchesTable.onResetPtpForSwitch = [this](const juce::Identifier &switchID)
        {
            resetPtpForSwitch(switchID);
        };

        switchesTable.onSwitchRemoved = [this](const juce::Identifier &switchID)
        {
            removeSwitch(switchID);
        };

        // Show any switches that already exist, e.g. restored with the project.
        update();
    }

    SwitchesComponent::~SwitchesComponent()
    {
        persistentTree.removeListener(this);
        dynamicTree.removeListener(this);
    }

    void SwitchesComponent::paint(juce::Graphics &g)
    {
        g.fillAll(juce::Colours::transparentWhite);
    }

    void SwitchesComponent::resized()
    {
        auto bounds{getLocalBounds()};
        auto titleRow{
            bounds.removeFromTop(Dimensions::NetworkSectionTitleHeight)
            .reduced(6, 0)
        };
        title.setBounds(titleRow.removeFromLeft(85));
        addSwitchButton.setBounds(titleRow.removeFromLeft(45).reduced(8));
        switchesTable.setBounds(bounds);
    }

    void SwitchesComponent::update()
    {
        switchesTable.update(switchesNode);
    }

    void SwitchesComponent::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property)
    {
        if (!isVisible()) return;

        if (treeWhosePropertyHasChanged.hasType(Utils::Identifiers::SwitchTreeType)) {
            update();
            handleAsyncUpdate();
        }
    }

    void SwitchesComponent::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &)
    {
        if (parentTree == switchesNode) {
            update();
        }
    }

    void SwitchesComponent::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &, int)
    {
        if (parentTree == switchesNode) {
            update();
        }
    }

    void SwitchesComponent::handleAsyncUpdate()
    {
        repaint();
    }

    void SwitchesComponent::addSwitch() const
    {
        juce::Random r;

        const auto switchID{juce::Identifier{Utils::Identifiers::SwitchIdentifierBase + juce::String{abs(r.nextInt())}}};

        switchList.setSwitch(switchID.toString(), {}, {}, {});
    }

    void SwitchesComponent::removeSwitch(const juce::Identifier &switchID) const
    {
        switchList.removeSwitch(switchID.toString());
    }

    void SwitchesComponent::resetPtpForSwitch(const juce::Identifier &switchID) const
    {
        switchList.requestPtpReset(switchID.toString());
    }

    void SwitchesComponent::updateSwitch(const juce::Identifier &switchID, const int col, const juce::String &content) const
    {
        const auto switchNode{switchesNode.getChildWithProperty("identifier", switchID.toString())};

        auto ip{switchNode.getProperty(ananas::Utils::Identifiers::SwitchIpPropertyID).toString()};

        auto username{switchNode.getProperty(ananas::Utils::Identifiers::SwitchUsernamePropertyID).toString()};

        auto password{switchNode.getProperty(ananas::Utils::Identifiers::SwitchPasswordPropertyID).toString()};

        // Update the appropriate property based on column ID
        switch (col) {
            case 1: ip = content;
                break;
            case 2: username = content;
                break;
            case 3: password = content;
                break;
            default: break;
        }

        switchList.setSwitch(switchID.toString(), ip, username, password);
    }

    //==========================================================================

    SwitchesComponent::SwitchesTable::SwitchesTable()
    {
        addAndMakeVisible(table);

        addColumn(TableColumns::SwitchesTableIpAddress);
        addColumn(TableColumns::SwitchesTableUsername);
        addColumn(TableColumns::SwitchesTablePassword);
        addColumn(TableColumns::SwitchesTableDrift);
        addColumn(TableColumns::SwitchesTableOffset);
        addColumn(TableColumns::SwitchesTableResetPTP);
        addColumn(TableColumns::SwitchesTableRemoveSwitch);

        table.setModel(this);
        table.setOutlineThickness(1);
    }

    void SwitchesComponent::SwitchesTable::update(const juce::ValueTree& switchesNode)
    {
        if (!isVisible() || isEditing) return;

        rows.clear();

        for (const auto &switchNode: switchesNode) {
            if (!switchNode.hasType(Utils::Identifiers::SwitchTreeType)) continue;

            Row row;
            row.id = switchNode.getProperty("identifier").toString();
            row.ip = switchNode.getProperty(Utils::Identifiers::SwitchIpPropertyID);
            row.username = switchNode.getProperty(Utils::Identifiers::SwitchUsernamePropertyID);
            row.password = switchNode.getProperty(Utils::Identifiers::SwitchPasswordPropertyID);
            row.freqDriftPPB = switchNode.getProperty(Utils::Identifiers::SwitchFreqDriftPropertyId);
            row.offsetNS = switchNode.getProperty(Utils::Identifiers::SwitchOffsetPropertyId);

            rows.add(row);
        }

        table.updateContent();
        repaint();
    }

    int SwitchesComponent::SwitchesTable::getNumRows()
    {
        return rows.size();
    }

    void SwitchesComponent::SwitchesTable::paintRowBackground(juce::Graphics &g, const int rowNumber, int width, int height, const bool rowIsSelected)
    {
        juce::ignoreUnused(width, height);

        if (rowIsSelected)
            g.fillAll(juce::Colours::lightblue);
        else if (rowNumber % 2 == 0)
            g.fillAll(juce::Colour(0xffeeeeee));
        else
            g.fillAll(juce::Colours::white);
    }

    void SwitchesComponent::SwitchesTable::paintCell(juce::Graphics &g, const int rowNumber, const int columnId, const int width, const int height, bool rowIsSelected)
    {
        juce::ignoreUnused(rowIsSelected);

        if (rowNumber < rows.size()) {
            const auto &[id, ip, username, password, drift, offset] = rows[rowNumber];
            juce::String text;

            switch (columnId) {
                case 4: text = Strings::formatWithThousandsSeparator(drift);
                    break;
                case 5: text = Strings::formatWithThousandsSeparator(offset);
                    break;
                default: break;
            }

            g.setColour(findColour(textColourId));
            g.setFont(juce::Font{juce::FontOptions{14.f}});
            g.drawText(text, 2, 0, width - 4, height, getJustification(columnId), true);
        }
    }

    void SwitchesComponent::SwitchesTable::resized()
    {
        table.setBounds(getLocalBounds().reduced(10));
    }

    juce::Component *SwitchesComponent::SwitchesTable::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                                                               Component *existingComponentToUpdate)
    {
        juce::ignoreUnused(isRowSelected);

        if (editableColumnIDs.contains(columnId)) {
            auto *textEditor = dynamic_cast<juce::TextEditor *>(existingComponentToUpdate);

            if (textEditor == nullptr) {
                textEditor = new juce::TextEditor{"Switch editor row " + juce::String{rowNumber} + " col " + juce::String{columnId}};
                textEditor->setMultiLine(false);
                textEditor->setReturnKeyStartsNewLine(false);
                textEditor->setWantsKeyboardFocus(true);
                textEditor->setMouseClickGrabsKeyboardFocus(true);
                textEditor->setExplicitFocusOrder(1);

                if (columnId == passwordColumnID) {
                    textEditor->setPasswordCharacter(L'•');
                }

                textEditor->onTextChange = [this]()
                {
                    isEditing = true;
                };

                textEditor->onFocusLost = [this, rowNumber, columnId]
                {
                    const auto *editor = dynamic_cast<juce::TextEditor *>(
                        table.getCellComponent(columnId, rowNumber)
                    );
                    if (editor != nullptr && onCellEdited) {
                        switch (columnId) {
                            case 1: rows[rowNumber].ip = editor->getText();
                                break;
                            case 2: rows[rowNumber].username = editor->getText();
                                break;
                            case 3: rows[rowNumber].password = editor->getText();
                                break;
                            default: break;
                        }
                        onCellEdited(rowNumber, columnId, editor->getText());
                    }
                    isEditing = false;
                };

                textEditor->onReturnKey = [textEditor]()
                {
                    textEditor->moveKeyboardFocusToSibling(true);
                };
            }

            switch (columnId) {
                case 1: textEditor->setText(rows[rowNumber].ip, juce::dontSendNotification);
                    break;
                case 2: textEditor->setText(rows[rowNumber].username, juce::dontSendNotification);
                    break;
                case 3: textEditor->setText(rows[rowNumber].password, juce::dontSendNotification);
                    break;
                default: break;
            }

            return textEditor;
        } else if (columnId == 6) {
            auto *button{dynamic_cast<juce::Button *>(existingComponentToUpdate)};

            if (button == nullptr) {
                button = new juce::TextButton{Strings::ResetSwitchButtonText};
                button->onClick = [this, rowNumber]
                {
                    handleResetPtpForSwitch(rows[rowNumber].id);
                };
            }

            return button;
        } else if (columnId == 7) {
            auto *button{dynamic_cast<juce::Button *>(existingComponentToUpdate)};

            if (button == nullptr) {
                button = new RemoveSwitchButton{Strings::RemoveSwitchButtonText};
                button->onClick = [this, rowNumber]
                {
                    handleRemoveSwitch(rows[rowNumber].id);
                };
            }

            return button;
        }

        return nullptr;
    }

    void SwitchesComponent::SwitchesTable::handleResetPtpForSwitch(const juce::Identifier &switchID) const
    {
        if (onResetPtpForSwitch)
            onResetPtpForSwitch(switchID);
    }

    void SwitchesComponent::SwitchesTable::handleRemoveSwitch(const juce::Identifier &switchID) const
    {
        if (onSwitchRemoved)
            onSwitchRemoved(switchID);
    }

    juce::Identifier SwitchesComponent::SwitchesTable::getSwitchID(const int rowNumber) const
    {
        return rows[rowNumber].id;
    }
} // ananas::UI
