#include "SpeakerTableComponent.h"
#include "../LayoutCheck.h"
#include "../WFSUtils.h"
#include "../LayoutFile.h"
#include "../IemLayout.h"

namespace ananas::WFS::UI
{
    namespace
    {
        using ::ananas::UI::TableColumns;

        constexpr auto ColumnFlags = juce::TableHeaderComponent::visible
                                     | juce::TableHeaderComponent::resizable
                                     | juce::TableHeaderComponent::appearsOnColumnMenu;

        enum ColumnIds
        {
            speakerColumnId = 1,
            xColumnId,
            yColumnId,
            zColumnId,
            facingColumnId,
            tiltColumnId,
            moduleColumnId,
            outputColumnId,
            problemsColumnId,
        };

        const TableColumns::ColumnHeader SpeakerColumn{speakerColumnId, "Speaker", 70, 30, -1, ColumnFlags, juce::Justification::centred};
        const TableColumns::ColumnHeader XColumn{xColumnId, "x (m)", 80, 30, -1, ColumnFlags, juce::Justification::centredRight};
        const TableColumns::ColumnHeader YColumn{yColumnId, "y (m)", 80, 30, -1, ColumnFlags, juce::Justification::centredRight};
        const TableColumns::ColumnHeader ZColumn{zColumnId, "z (m)", 80, 30, -1, ColumnFlags, juce::Justification::centredRight};
        // Non-ASCII, so from UTF-8 (a plain char* label must be ASCII).
        const auto FacingLabel = juce::String::fromUTF8("Facing (\xc2\xb0)");
        const auto TiltLabel = juce::String::fromUTF8("Tilt (\xc2\xb0)");
        const TableColumns::ColumnHeader FacingColumn{facingColumnId, FacingLabel, 85, 30, -1, ColumnFlags, juce::Justification::centredRight};
        const TableColumns::ColumnHeader TiltColumn{tiltColumnId, TiltLabel, 75, 30, -1, ColumnFlags, juce::Justification::centredRight};
        const TableColumns::ColumnHeader ModuleColumn{moduleColumnId, "Module", 200, 30, -1, ColumnFlags, juce::Justification::centredLeft};
        const TableColumns::ColumnHeader OutputColumn{outputColumnId, "Output", 70, 30, -1, ColumnFlags, juce::Justification::centred};
        const TableColumns::ColumnHeader ProblemsColumn{problemsColumnId, "Problems", 400, 30, -1, ColumnFlags, juce::Justification::centredLeft};

        constexpr auto Info = static_cast<int>(LayoutProblem::Severity::info);
        constexpr auto Warning = static_cast<int>(LayoutProblem::Severity::warning);

        // Layout mode selector item IDs (0 is reserved by ComboBox).
        constexpr int LinearArrayItemId = 1;
        constexpr int CustomItemId = 2;

        // Module cell: the first item means "no module".
        constexpr int NoModuleItemId = 1;

        juce::String formatMetres(const float value)
        {
            return juce::String{value, 3};
        }

        juce::String formatDegrees(const float value)
        {
            return juce::String{value, 1};
        }

        bool isEditableColumn(const int columnId)
        {
            return columnId >= xColumnId && columnId <= outputColumnId;
        }
    }

    //==========================================================================
    // Editable cells. JUCE reuses cell components for other rows, so each
    // cell keeps its current row, and its callbacks read it from there.

    // A number (x, y, z in metres; facing or tilt in degrees), committed on
    // return or when the editor loses focus.
    class SpeakerTableComponent::Table::NumberCell final : public juce::TextEditor
    {
    public:
        explicit NumberCell(Table &t) : table(t)
        {
            setMultiLine(false);
            setJustification(juce::Justification::centredRight);
            setInputRestrictions(0, "-0123456789.");
            onReturnKey = [this] { commit(); };
            onFocusLost = [this] { commit(); };
            onEscapeKey = [this] { show(row, column); };
        }

        void show(const int rowNumber, const int columnId)
        {
            row = rowNumber;
            column = columnId;
            // Don't overwrite what the user is typing.
            if (hasKeyboardFocus(true)) return;
            showValue();
        }

    private:
        void showValue()
        {
            const auto &r = table.rows[static_cast<size_t>(row)];
            switch (column) {
                case xColumnId: setText(formatMetres(r.x), juce::dontSendNotification);
                    break;
                case yColumnId: setText(formatMetres(r.y), juce::dontSendNotification);
                    break;
                case zColumnId: setText(formatMetres(r.z), juce::dontSendNotification);
                    break;
                case facingColumnId: setText(formatDegrees(r.facing.facingDegrees()), juce::dontSendNotification);
                    break;
                case tiltColumnId: setText(formatDegrees(r.facing.tiltDegrees()), juce::dontSendNotification);
                    break;
                default: break;
            }
        }

        void commit()
        {
            if (row >= table.getNumRows()) return;

            const auto &r = table.rows[static_cast<size_t>(row)];
            const auto text = getText().trim();
            if (text.isEmpty() || !text.containsAnyOf("0123456789")) {
                showValue();
                return;
            }

            const auto value = text.getFloatValue();
            switch (column) {
                case xColumnId:
                    if (table.onPositionEdited) table.onPositionEdited(r.index, {value, r.y});
                    break;
                case yColumnId:
                    if (table.onPositionEdited) table.onPositionEdited(r.index, {r.x, value});
                    break;
                case zColumnId:
                    if (table.onHeightEdited) table.onHeightEdited(r.index, value);
                    break;
                case facingColumnId:
                    if (table.onFacingEdited) table.onFacingEdited(r.index, Direction::fromAngles(value, r.facing.tiltDegrees()));
                    break;
                case tiltColumnId:
                    if (table.onFacingEdited) table.onFacingEdited(r.index, Direction::fromAngles(r.facing.facingDegrees(), value));
                    break;
                default: break;
            }
        }

        Table &table;
        int row = 0;
        int column = xColumnId;
    };

    // The module driving the speaker: the known modules, plus the current one
    // if it isn't known.
    class SpeakerTableComponent::Table::ModuleCell final : public juce::ComboBox
    {
    public:
        explicit ModuleCell(Table &t) : table(t)
        {
            onChange = [this] { commit(); };
        }

        void show(const int rowNumber)
        {
            row = rowNumber;
            const auto &r = table.rows[static_cast<size_t>(row)];

            clear(juce::dontSendNotification);
            ips.clear();

            addItem(juce::String::fromUTF8("–"), NoModuleItemId);
            ips.add({});

            for (const auto &m: table.modules) {
                ips.add(m.ip);
                addItem(m.label, ips.size());
            }
            if (r.moduleIp.isNotEmpty() && !ips.contains(r.moduleIp)) {
                ips.add(r.moduleIp);
                addItem(r.moduleIp + " (unknown)", ips.size());
            }

            setSelectedId(ips.indexOf(r.moduleIp) + 1, juce::dontSendNotification);
        }

    private:
        void commit()
        {
            if (row >= table.getNumRows() || !table.onModuleEdited) return;

            const auto &r = table.rows[static_cast<size_t>(row)];
            const auto ip = ips[getSelectedId() - 1];
            if (ip == r.moduleIp) return;

            // Keep the output if the new module has it; start at 0 otherwise.
            auto output = -1;
            if (ip.isNotEmpty()) {
                const auto *m = table.findModule(ip);
                output = m != nullptr && r.output >= 0 && r.output < m->numOutputs ? r.output : 0;
            }
            table.onModuleEdited(r.index, ip, output);
        }

        Table &table;
        int row = 0;
        // The IP of each item, by item ID - 1.
        juce::StringArray ips;
    };

    // The module output driving the speaker: the outputs the module has, plus
    // the current one if it doesn't have it.
    class SpeakerTableComponent::Table::OutputCell final : public juce::ComboBox
    {
    public:
        explicit OutputCell(Table &t) : table(t)
        {
            onChange = [this] { commit(); };
        }

        void show(const int rowNumber)
        {
            row = rowNumber;
            const auto &r = table.rows[static_cast<size_t>(row)];

            clear(juce::dontSendNotification);
            setEnabled(r.moduleIp.isNotEmpty());
            if (r.moduleIp.isEmpty()) return;

            const auto *m = table.findModule(r.moduleIp);
            const auto numOutputs = m != nullptr ? m->numOutputs : 0;
            // Item ID = output + 1.
            for (int output = 0; output < numOutputs; ++output) {
                addItem(juce::String{output}, output + 1);
            }
            if (r.output >= numOutputs) {
                addItem(juce::String{r.output} + " (doesn't exist)", r.output + 1);
            }

            setSelectedId(r.output + 1, juce::dontSendNotification);
        }

    private:
        void commit()
        {
            if (row >= table.getNumRows() || !table.onModuleEdited) return;

            const auto &r = table.rows[static_cast<size_t>(row)];
            const auto output = getSelectedId() - 1;
            if (output < 0 || output == r.output) return;
            table.onModuleEdited(r.index, r.moduleIp, output);
        }

        Table &table;
        int row = 0;
    };

    //==========================================================================

    SpeakerTableComponent::SpeakerTableComponent(juce::ValueTree &dynamicTree,
                                                 juce::ValueTree &persistentTree,
                                                 CustomLayout &customLayoutRef,
                                                 std::function<void(LayoutMode)> onLayoutModeSelectedCallback,
                                                 std::function<void()> onConvertToCustomLayoutCallback,
                                                 std::function<const SpeakerLayout &()> getLayoutToSaveCallback,
                                                 std::function<void(std::vector<Speaker>)> onLayoutLoadedCallback,
                                                 std::function<juce::Point<float>()> getListenerPositionCallback)
        : customLayout(customLayoutRef),
          onLayoutModeSelected(std::move(onLayoutModeSelectedCallback)),
          onConvertToCustomLayout(std::move(onConvertToCustomLayoutCallback)),
          getLayoutToSave(std::move(getLayoutToSaveCallback)),
          onLayoutLoaded(std::move(onLayoutLoadedCallback)),
          getListenerPosition(std::move(getListenerPositionCallback)),
          speakerLayoutNode(dynamicTree.getChildWithName(Identifiers::SpeakerLayoutTreeType)),
          layoutNode(persistentTree.getChildWithName(Identifiers::LayoutTreeType)),
          modulesNode(persistentTree.getChildWithName(::ananas::Utils::Identifiers::ModulesParamID))
    {
        addAndMakeVisible(title);
        addAndMakeVisible(layoutModeLabel);
        addAndMakeVisible(layoutModeSelector);
        addAndMakeVisible(convertButton);
        addAndMakeVisible(saveButton);
        addAndMakeVisible(loadButton);
        addChildComponent(addSpeakerButton);
        addChildComponent(removeSpeakersButton);
        addChildComponent(assignModuleButton);
        addAndMakeVisible(table);
        addAndMakeVisible(layoutView);
        addAndMakeVisible(moduleProblemsLabel);

        title.setFont(juce::Font{juce::FontOptions{18.f, juce::Font::bold}});
        title.setJustificationType(juce::Justification::centredLeft);
        title.setText("Speakers", juce::dontSendNotification);

        layoutModeLabel.setText("Layout", juce::dontSendNotification);
        layoutModeLabel.setJustificationType(juce::Justification::centredRight);
        layoutModeSelector.addItem("Linear array", LinearArrayItemId);
        layoutModeSelector.addItem("Custom", CustomItemId);
        layoutModeSelector.onChange = [this]
        {
            if (onLayoutModeSelected) {
                onLayoutModeSelected(layoutModeSelector.getSelectedId() == CustomItemId ? LayoutMode::custom
                                                                                          : LayoutMode::linearArray);
            }
        };

        convertButton.setButtonText("Copy linear array to custom");
        convertButton.setTooltip("Replaces the custom layout with the speakers of the linear array, "
                                 "including their modules, and switches to it.");
        convertButton.onClick = [this]
        {
            confirmReplacingCustomLayout("The custom layout's speakers will be replaced by those of the linear array.",
                                         [this] { if (onConvertToCustomLayout) onConvertToCustomLayout(); });
        };

        saveButton.setButtonText("Save layout...");
        saveButton.setTooltip("Saves the speakers in use to a speaker layout file.");
        saveButton.onClick = [this] { saveLayout(); };

        loadButton.setButtonText("Load layout...");
        loadButton.setTooltip("Loads a speaker layout file (Ananas, or an IEM loudspeaker layout) into the "
                              "custom layout, and switches to it.");
        loadButton.onClick = [this] { loadLayout(); };

        addSpeakerButton.setButtonText("Add speaker");
        addSpeakerButton.onClick = [this] { customLayout.addSpeaker(); };

        assignModuleButton.setButtonText("Assign to module...");
        assignModuleButton.setTooltip("Assigns a module to the selected speakers: in table order, they get its "
                                      "outputs 0, 1, 2, ...");
        assignModuleButton.onClick = [this] { showAssignModuleMenu(); };

        removeSpeakersButton.setButtonText("Remove selected");
        removeSpeakersButton.onClick = [this]
        {
            // From the last, so the indices of the others stay valid.
            const auto selected = table.getSelectedRows();
            for (int i = selected.size() - 1; i >= 0; --i) {
                customLayout.removeSpeaker(selected[i]);
            }
        };

        table.onPositionEdited = [this](const int index, const juce::Point<float> position)
        {
            customLayout.setPosition(index, position);
        };
        table.onHeightEdited = [this](const int index, const float z)
        {
            customLayout.setHeight(index, z);
        };
        table.onFacingEdited = [this](const int index, const Direction facing)
        {
            customLayout.setFacing(index, facing);
        };
        table.onModuleEdited = [this](const int index, const juce::String &ip, const int output)
        {
            customLayout.setModule(index, ip, output);
        };

        moduleProblemsLabel.setJustificationType(juce::Justification::topLeft);
        moduleProblemsLabel.setColour(juce::Label::textColourId, juce::Colours::darkorange.darker(.3f));

        // Selection is shared between the table and the view.
        table.onSelectionChanged = [this](const juce::SparseSet<int> &selection)
        {
            layoutView.setSelection(selection);
        };
        layoutView.onSpeakerClicked = [this](const int index, const juce::ModifierKeys &modifiers)
        {
            auto selection = table.getSelectedRows();
            if (modifiers.isCommandDown() || modifiers.isShiftDown()) {
                // Add or remove one speaker; empty space changes nothing.
                if (index < 0) return;
                if (selection.contains(index)) selection.removeRange({index, index + 1});
                else selection.addRange({index, index + 1});
            } else {
                selection.clear();
                if (index >= 0) selection.addRange({index, index + 1});
            }
            table.setSelectedRows(selection);
        };

        speakerLayoutNode.addListener(this);
        layoutNode.addListener(this);
        modulesNode.addListener(this);
        update();
    }

    SpeakerTableComponent::~SpeakerTableComponent()
    {
        speakerLayoutNode.removeListener(this);
        layoutNode.removeListener(this);
        modulesNode.removeListener(this);
        cancelPendingUpdate();
        stopTimer();
    }

    void SpeakerTableComponent::resized()
    {
        auto bounds = getLocalBounds().reduced(10);

        auto top = bounds.removeFromTop(30);
        title.setBounds(top.removeFromLeft(120));
        loadButton.setBounds(top.removeFromRight(130).reduced(2));
        saveButton.setBounds(top.removeFromRight(130).reduced(2));
        top.removeFromRight(10);
        convertButton.setBounds(top.removeFromRight(220).reduced(2));
        top.removeFromRight(10);
        layoutModeSelector.setBounds(top.removeFromRight(140).reduced(2));
        layoutModeLabel.setBounds(top.removeFromRight(60));

        bounds.removeFromTop(5);
        if (addSpeakerButton.isVisible()) {
            auto buttons = bounds.removeFromBottom(30);
            addSpeakerButton.setBounds(buttons.removeFromLeft(120).reduced(2));
            removeSpeakersButton.setBounds(buttons.removeFromLeft(140).reduced(2));
            buttons.removeFromLeft(10);
            assignModuleButton.setBounds(buttons.removeFromLeft(160).reduced(2));
            bounds.removeFromBottom(5);
        }

        moduleProblemsLabel.setBounds(bounds.removeFromBottom(moduleProblemsLabel.getText().isEmpty() ? 0 : 80));

        // The table on the left, the view on the right.
        layoutView.setBounds(bounds.removeFromRight(juce::roundToInt(static_cast<float>(bounds.getWidth()) * .42f)));
        bounds.removeFromRight(10);
        table.setBounds(bounds);
    }

    void SpeakerTableComponent::timerCallback()
    {
        if (isShowing() && getListenerPosition) {
            layoutView.setListener(getListenerPosition());
        }
    }

    // A change to one speaker comes as several notifications (one per
    // property); rebuild the table once, after all of them.
    void SpeakerTableComponent::valueTreePropertyChanged(juce::ValueTree &, const juce::Identifier &)
    {
        triggerAsyncUpdate();
    }

    void SpeakerTableComponent::valueTreeChildAdded(juce::ValueTree &, juce::ValueTree &)
    {
        triggerAsyncUpdate();
    }

    void SpeakerTableComponent::valueTreeChildRemoved(juce::ValueTree &, juce::ValueTree &, int)
    {
        triggerAsyncUpdate();
    }

    void SpeakerTableComponent::handleAsyncUpdate()
    {
        update();
    }

    void SpeakerTableComponent::update()
    {
        const auto isCustom = layoutModeFromString(layoutNode[Identifiers::LayoutModePropertyID].toString())
                              == LayoutMode::custom;

        layoutModeSelector.setSelectedId(isCustom ? CustomItemId : LinearArrayItemId, juce::dontSendNotification);
        addSpeakerButton.setVisible(isCustom);
        removeSpeakersButton.setVisible(isCustom);
        assignModuleButton.setVisible(isCustom);

        std::vector<ModuleOption> modules;
        // All known modules, so a speaker can keep (and be given) a module
        // that's offline; checkLayout warns about those.
        for (const auto &module: modulesNode) {
            const auto ip = module[::ananas::Utils::Identifiers::ModuleIpPropertyID].toString();
            const auto type = static_cast<::ananas::Utils::FirmwareType>(
                static_cast<int>(module[::ananas::Utils::Identifiers::ModuleFirmwareTypePropertyID]));
            const int numOutputs = module[::ananas::Utils::Identifiers::ModuleNumSpeakersPropertyID];
            const bool isConnected = module[::ananas::Utils::Identifiers::ModuleIsConnectedPropertyID];
            modules.push_back({ip,
                               ip + " (" + ::ananas::Utils::FirmwareTypeToString(type)
                               + (isConnected ? ")" : ", not connected)"),
                               numOutputs});
        }

        std::vector<Table::Row> rows;
        for (const auto &speaker: speakerLayoutNode) {
            if (!speaker.hasType(Identifiers::SpeakerTreeType)) continue;

            Table::Row row;
            row.index = speaker[Identifiers::SpeakerIndexPropertyID];
            row.x = speaker[Identifiers::SpeakerXPropertyID];
            row.y = speaker[Identifiers::SpeakerYPropertyID];
            row.z = speaker[Identifiers::SpeakerZPropertyID];
            row.facing = Direction{speaker[Identifiers::SpeakerFacingXPropertyID],
                                   speaker[Identifiers::SpeakerFacingYPropertyID],
                                   speaker[Identifiers::SpeakerFacingZPropertyID]}.normalised();
            row.moduleIp = speaker[Identifiers::SpeakerModuleIpPropertyID].toString();
            row.output = speaker[Identifiers::SpeakerOutputPropertyID];
            row.severity = speaker[Identifiers::SpeakerSeverityPropertyID];
            row.problems = juce::StringArray::fromLines(speaker[Identifiers::SpeakerProblemsPropertyID].toString());
            row.problems.removeEmptyStrings();
            rows.push_back(std::move(row));
        }

        std::vector<LayoutView::ViewSpeaker> viewSpeakers;
        for (const auto &row: rows) {
            viewSpeakers.push_back({row.index, {row.x, row.y}, row.z, row.facing, row.moduleIp, row.severity});
        }
        layoutView.setSpeakers(std::move(viewSpeakers));
        layoutView.setSelection(table.getSelectedRows());
        if (getListenerPosition) layoutView.setListener(getListenerPosition());
        if (!isTimerRunning()) startTimerHz(5);

        moduleOptions = modules;
        table.setRows(std::move(rows), std::move(modules), isCustom);

        const auto moduleProblems = speakerLayoutNode[Identifiers::SpeakerLayoutModuleProblemsPropertyID].toString();
        moduleProblemsLabel.setText(moduleProblems, juce::dontSendNotification);
        moduleProblemsLabel.setTooltip(moduleProblems);
        resized();
    }

    void SpeakerTableComponent::showAssignModuleMenu()
    {
        const auto selected = table.getSelectedRows();
        if (selected.isEmpty()) {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                             juce::MessageBoxIconType::InfoIcon,
                                             "Assign to module",
                                             "Select the speakers first (Shift- or Cmd-click for several)."),
                                         nullptr);
            return;
        }

        // Item IDs: 1 for "no module", module i at i + 2 (0 means cancelled).
        constexpr int noModuleItemId = 1;
        juce::PopupMenu menu;
        menu.addSectionHeader(juce::String{selected.size()} + " speaker" + (selected.size() == 1 ? "" : "s"));
        for (size_t i = 0; i < moduleOptions.size(); ++i) {
            const auto &m = moduleOptions[i];
            menu.addItem(static_cast<int>(i) + 2, m.label + ", " + juce::String{m.numOutputs} + " outputs");
        }
        menu.addSeparator();
        menu.addItem(noModuleItemId, "No module");

        std::vector<int> indices;
        for (int i = 0; i < selected.size(); ++i) {
            indices.push_back(selected[i]);
        }

        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&assignModuleButton),
                           [safeThis = juce::Component::SafePointer{this}, indices, options = moduleOptions](const int id)
                           {
                               if (safeThis == nullptr || id == 0) return;

                               const auto *module = id >= 2 ? &options[static_cast<size_t>(id - 2)] : nullptr;
                               const auto ip = module != nullptr ? module->ip : juce::String{};
                               const auto numOutputs = module != nullptr ? module->numOutputs : 0;

                               const auto numAssigned = safeThis->customLayout.assignOutputs(indices, ip, numOutputs);

                               if (module != nullptr && numAssigned < static_cast<int>(indices.size())) {
                                   juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                                    juce::MessageBoxIconType::InfoIcon,
                                                                    "Assign to module",
                                                                    module->label + " has " + juce::String{numOutputs}
                                                                    + " outputs, so only the first " + juce::String{numAssigned}
                                                                    + " of the " + juce::String{indices.size()}
                                                                    + " selected speakers were assigned."),
                                                                nullptr);
                               }
                           });
    }

    void SpeakerTableComponent::confirmReplacingCustomLayout(const juce::String &message, std::function<void()> replace)
    {
        if (customLayout.getLayout().getSpeakers().empty()) {
            replace();
            return;
        }

        const auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Replace the custom layout?")
                .withMessage(message)
                .withButton("Replace")
                .withButton("Cancel")
                .withAssociatedComponent(this);

        juce::AlertWindow::showAsync(options, [safeThis = juce::Component::SafePointer{this},
                                               action = std::move(replace)](const int result)
        {
            // The first button returns 1.
            if (result == 1 && safeThis != nullptr) {
                action();
            }
        });
    }

    void SpeakerTableComponent::saveLayout()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save speaker layout",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("speaker-layout.json"),
            "*.json");

        const auto flags = juce::FileBrowserComponent::saveMode
                           | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::warnAboutOverwriting;

        fileChooser->launchAsync(flags, [safeThis = juce::Component::SafePointer{this}](const juce::FileChooser &chooser)
        {
            const auto file = chooser.getResult();
            if (safeThis == nullptr || file == juce::File{} || !safeThis->getLayoutToSave) return;

            const auto json = LayoutFile::toJson(safeThis->getLayoutToSave());
            if (!file.withFileExtension("json").replaceWithText(json)) {
                juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                 juce::MessageBoxIconType::WarningIcon,
                                                 "Couldn't save the speaker layout",
                                                 "Writing " + file.getFullPathName() + " failed."),
                                             nullptr);
            }
        });
    }

    void SpeakerTableComponent::loadLayout()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load speaker layout",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.json");

        const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync(flags, [safeThis = juce::Component::SafePointer{this}](const juce::FileChooser &chooser)
        {
            const auto file = chooser.getResult();
            if (safeThis == nullptr || file == juce::File{}) return;

            const auto text = file.loadFileAsString();

            // IEM layouts give directions; they need a radius and a centre first.
            if (const auto json = juce::JSON::parse(text); IemLayout::isIemLayout(json)) {
                safeThis->askIemPlacement(file.getFileName(), json);
                return;
            }

            safeThis->applyLoadedLayout(LayoutFile::fromJson(text), file.getFileName());
        });
    }

    void SpeakerTableComponent::askIemPlacement(const juce::String &fileName, const juce::var &json)
    {
        const auto listener = getListenerPosition ? getListenerPosition() : juce::Point<float>{0.f, -2.f};
        // Layouts with all radii 1 only give directions: ask for the real
        // distance. Others are in metres already: ask for a scale.
        const auto unitRadii = IemLayout::hasUnitRadii(json);

        auto *window = new juce::AlertWindow(
            "Import " + fileName,
            juce::String{"An IEM loudspeaker layout gives directions as seen from the listening position. "}
            + (unitRadii ? "All its radii are 1, so enter the real distance of the speakers from the centre. "
                         : "Its radii are multiplied by the scale below. ")
            + "The speakers are placed around the centre, facing it, without modules.",
            juce::MessageBoxIconType::QuestionIcon);

        window->addTextEditor("radius", unitRadii ? "2.0" : "1.0", unitRadii ? "Radius (m)" : "Scale radii by");
        window->addTextEditor("x", juce::String{listener.x, 3}, "Centre x (m)");
        window->addTextEditor("y", juce::String{listener.y, 3}, "Centre y (m)");
        window->addButton("Import", 1, juce::KeyPress{juce::KeyPress::returnKey});
        window->addButton("Cancel", 0, juce::KeyPress{juce::KeyPress::escapeKey});

        window->enterModalState(true, juce::ModalCallbackFunction::create(
                                    [safeThis = juce::Component::SafePointer{this}, window, json, fileName](const int result)
                                    {
                                        // Runs before the window is deleted.
                                        if (result != 1 || safeThis == nullptr) return;

                                        const auto scale = window->getTextEditorContents("radius").getFloatValue();
                                        const juce::Point<float> centre{
                                            window->getTextEditorContents("x").getFloatValue(),
                                            window->getTextEditorContents("y").getFloatValue()
                                        };
                                        safeThis->applyLoadedLayout(IemLayout::read(json, centre, scale), fileName);
                                    }),
                                true);
    }

    void SpeakerTableComponent::applyLoadedLayout(LayoutFile::ReadResult result, const juce::String &fileName)
    {
        if (!result.wasLoaded()) {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                             juce::MessageBoxIconType::WarningIcon,
                                             "Couldn't load " + fileName,
                                             result.errors.joinIntoString("\n")),
                                         nullptr);
            return;
        }

        confirmReplacingCustomLayout(
            "The custom layout's speakers will be replaced by those in " + fileName + ".",
            [safeThis = juce::Component::SafePointer{this}, speakers = std::move(result.speakers),
                warnings = result.warnings, fileName]
            {
                if (safeThis == nullptr || !safeThis->onLayoutLoaded) return;
                safeThis->onLayoutLoaded(speakers);

                if (!warnings.isEmpty()) {
                    juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                     juce::MessageBoxIconType::InfoIcon,
                                                     "Loaded " + fileName,
                                                     warnings.joinIntoString("\n")),
                                                 nullptr);
                }
            });
    }

    //==========================================================================

    SpeakerTableComponent::Table::Table()
    {
        addAndMakeVisible(table);

        addColumn(SpeakerColumn);
        addColumn(XColumn);
        addColumn(YColumn);
        addColumn(ZColumn);
        addColumn(FacingColumn);
        addColumn(TiltColumn);
        addColumn(ModuleColumn);
        addColumn(OutputColumn);
        addColumn(ProblemsColumn);

        table.setModel(this);
        table.setOutlineThickness(1);
        table.setMultipleSelectionEnabled(true);
    }

    void SpeakerTableComponent::Table::setRows(std::vector<Row> newRows, std::vector<ModuleOption> newModules,
                                               const bool isEditable)
    {
        rows = std::move(newRows);
        modules = std::move(newModules);
        editable = isEditable;
        table.updateContent();
        table.repaint();
    }

    juce::SparseSet<int> SpeakerTableComponent::Table::getSelectedRows() const
    {
        return table.getSelectedRows();
    }

    void SpeakerTableComponent::Table::setSelectedRows(const juce::SparseSet<int> &selection)
    {
        table.setSelectedRows(selection);
    }

    void SpeakerTableComponent::Table::selectedRowsChanged(int)
    {
        if (onSelectionChanged) onSelectionChanged(table.getSelectedRows());
    }

    int SpeakerTableComponent::Table::getNumRows()
    {
        return static_cast<int>(rows.size());
    }

    void SpeakerTableComponent::Table::paintRowBackground(juce::Graphics &g, const int rowNumber, int, int,
                                                          const bool rowIsSelected)
    {
        if (rowIsSelected) {
            g.fillAll(juce::Colours::lightblue);
            return;
        }

        g.fillAll(rowNumber % 2 == 0 ? juce::Colour{0xffeeeeee} : juce::Colours::white);

        // Tint rows with problems, so they stand out in a long list.
        if (rowNumber < getNumRows() && rows[static_cast<size_t>(rowNumber)].severity >= Warning) {
            g.fillAll(findColour(warningColourId).withAlpha(.25f));
        }
    }

    void SpeakerTableComponent::Table::paintCell(juce::Graphics &g, const int rowNumber, const int columnId,
                                                 const int width, const int height, bool)
    {
        if (rowNumber >= getNumRows()) return;

        // Editable cells are drawn by their components.
        if (editable && isEditableColumn(columnId)) return;

        const auto &row = rows[static_cast<size_t>(rowNumber)];
        juce::String text;

        switch (columnId) {
            case speakerColumnId: text = juce::String{row.index + 1};
                break;
            case xColumnId: text = formatMetres(row.x);
                break;
            case yColumnId: text = formatMetres(row.y);
                break;
            case zColumnId: text = formatMetres(row.z);
                break;
            case facingColumnId: text = formatDegrees(row.facing.facingDegrees());
                break;
            case tiltColumnId: text = formatDegrees(row.facing.tiltDegrees());
                break;
            case moduleColumnId: text = row.moduleIp.isEmpty() ? juce::String::fromUTF8("–") : row.moduleIp;
                break;
            case outputColumnId: text = row.output < 0 ? juce::String::fromUTF8("–") : juce::String{row.output};
                break;
            case problemsColumnId:
                if (!row.problems.isEmpty()) {
                    text = row.problems[0];
                    if (row.problems.size() > 1) {
                        text << " (+" << (row.problems.size() - 1) << " more)";
                    }
                }
                break;
            default: break;
        }

        const auto isInfoOnly = columnId == problemsColumnId && row.severity == Info;
        g.setColour(findColour(textColourId).withAlpha(isInfoOnly ? .5f : 1.f));
        g.setFont(juce::Font{juce::FontOptions{14.f}});
        g.drawText(text, 2, 0, width - 4, height, getJustification(columnId), true);
    }

    juce::Component *SpeakerTableComponent::Table::refreshComponentForCell(const int rowNumber, const int columnId, bool,
                                                                           juce::Component *existing)
    {
        const auto wantsComponent = editable && rowNumber < getNumRows()
                                    && isEditableColumn(columnId);

        if (!wantsComponent) {
            delete existing;
            return nullptr;
        }

        if (columnId != moduleColumnId && columnId != outputColumnId) {
            auto *cell = dynamic_cast<NumberCell *>(existing);
            if (cell == nullptr) {
                delete existing;
                cell = new NumberCell{*this};
            }
            cell->show(rowNumber, columnId);
            return cell;
        }

        if (columnId == moduleColumnId) {
            auto *cell = dynamic_cast<ModuleCell *>(existing);
            if (cell == nullptr) {
                delete existing;
                cell = new ModuleCell{*this};
            }
            cell->show(rowNumber);
            return cell;
        }

        auto *cell = dynamic_cast<OutputCell *>(existing);
        if (cell == nullptr) {
            delete existing;
            cell = new OutputCell{*this};
        }
        cell->show(rowNumber);
        return cell;
    }

    juce::String SpeakerTableComponent::Table::getCellTooltip(const int rowNumber, int)
    {
        if (rowNumber >= getNumRows()) return {};
        return rows[static_cast<size_t>(rowNumber)].problems.joinIntoString("\n");
    }

    void SpeakerTableComponent::Table::resized()
    {
        table.setBounds(getLocalBounds());
    }

    const SpeakerTableComponent::ModuleOption *SpeakerTableComponent::Table::findModule(const juce::String &ip) const
    {
        for (const auto &m: modules) {
            if (m.ip == ip) return &m;
        }
        return nullptr;
    }
}
