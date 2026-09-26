#include "SwitchesComponent.h"
#include <AnanasUtils.h>

namespace ananas::UI
{
    SwitchesComponent::SwitchesComponent(juce::ValueTree &dynamicTreeRef, juce::ValueTree &persistentTreeRef)
        : addSwitchButton(Strings::AddSwitchButtonName),
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
        update(persistentTree.getProperty(Utils::Identifiers::SwitchesParamID));
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

    void SwitchesComponent::update(const juce::var &var)
    {
        switchesTable.update(var);
    }

    void SwitchesComponent::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property)
    {
        if (!isVisible()) return;

        if (property == Utils::Identifiers::SwitchesParamID) {
            update(treeWhosePropertyHasChanged[property]);
            handleAsyncUpdate();
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

        updateSwitch(switchID, 0, "");
    }

    void SwitchesComponent::removeSwitch(const juce::Identifier &switchID) const
    {
        const auto switchesVar{persistentTree.getProperty(Utils::Identifiers::SwitchesParamID)};

        const auto switchesObject{switchesVar.getDynamicObject()};
        if (switchesObject == nullptr) return;

        const auto switchVar = switchesObject->getProperty(switchID);
        if (!switchVar.isObject()) return;
        switchVar.getDynamicObject()->setProperty(Utils::Identifiers::SwitchShouldRemovePropertyID, true);
        switchesObject->setProperty(switchID, switchVar);

        persistentTree.setProperty(Utils::Identifiers::SwitchesParamID, switchesVar, nullptr);
        persistentTree.sendPropertyChangeMessage(Utils::Identifiers::SwitchesParamID);
    }

    void SwitchesComponent::resetPtpForSwitch(const juce::Identifier &switchID) const
    {
        // The dynamic tree only holds the switches once the server has
        // broadcast them, so fall back to the persistent tree.
        auto *tree{&dynamicTree};
        if (!tree->getProperty(Utils::Identifiers::SwitchesParamID).isObject()) {
            tree = &persistentTree;
        }

        const auto switchesVar{tree->getProperty(Utils::Identifiers::SwitchesParamID)};

        const auto switchesObject{switchesVar.getDynamicObject()};
        if (switchesObject == nullptr) return;

        const auto switchVar = switchesObject->getProperty(switchID);
        if (!switchVar.isObject()) return;
        switchVar.getDynamicObject()->setProperty(Utils::Identifiers::SwitchShouldResetPtpPropertyID, true);
        switchesObject->setProperty(switchID, switchVar);

        tree->setProperty(Utils::Identifiers::SwitchesParamID, switchesVar, nullptr);
        tree->sendPropertyChangeMessage(Utils::Identifiers::SwitchesParamID);
    }

    void SwitchesComponent::updateSwitch(const juce::Identifier &switchID, const int col, const juce::String &content) const
    {
        auto switchesVar{persistentTree.getProperty(Utils::Identifiers::SwitchesParamID)};

        if (!switchesVar.isObject()) {
            switchesVar = new juce::DynamicObject;
        }

        const auto switchesObject{switchesVar.getDynamicObject()};

        auto rowVar = switchesObject->getProperty(switchID);
        if (!rowVar.isObject()) {
            rowVar = new juce::DynamicObject;
            rowVar.getDynamicObject()->setProperty(Utils::Identifiers::SwitchIpPropertyID, "");
            rowVar.getDynamicObject()->setProperty(Utils::Identifiers::SwitchUsernamePropertyID, "");
            rowVar.getDynamicObject()->setProperty(Utils::Identifiers::SwitchPasswordPropertyID, "");
            switchesObject->setProperty(switchID, rowVar);
        }

        const auto rowObject = rowVar.getDynamicObject();

        // Update the appropriate property based on column ID
        switch (col) {
            case 1: rowObject->setProperty(Utils::Identifiers::SwitchIpPropertyID, content);
                break;
            case 2: rowObject->setProperty(Utils::Identifiers::SwitchUsernamePropertyID, content);
                break;
            case 3: rowObject->setProperty(Utils::Identifiers::SwitchPasswordPropertyID, content);
                break;
            default: break;
        }

        // Write back to trigger change notification
        persistentTree.setProperty(Utils::Identifiers::SwitchesParamID, switchesVar, nullptr);
        persistentTree.sendPropertyChangeMessage(Utils::Identifiers::SwitchesParamID);
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

    void SwitchesComponent::SwitchesTable::update(const juce::var &var)
    {
        if (!isVisible() || isEditing) return;

        rows.clear();

        if (auto *obj = var.getDynamicObject()) {
            for (const auto &prop: obj->getProperties()) {
                Row row;

                if (const auto *theSwitch = prop.value.getDynamicObject()) {
                    row.id = prop.name;
                    row.ip = theSwitch->getProperty(Utils::Identifiers::SwitchIpPropertyID);
                    row.username = theSwitch->getProperty(Utils::Identifiers::SwitchUsernamePropertyID);
                    row.password = theSwitch->getProperty(Utils::Identifiers::SwitchPasswordPropertyID);
                    row.freqDriftPPB = theSwitch->getProperty(Utils::Identifiers::SwitchFreqDriftPropertyId);
                    row.offsetNS = theSwitch->getProperty(Utils::Identifiers::SwitchOffsetPropertyId);
                }

                rows.add(row);
            }
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
