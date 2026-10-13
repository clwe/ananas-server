#include "SpeakerTableComponent.h"
#include "../LayoutCheck.h"
#include "../WFSUtils.h"

namespace ananas::WFS::UI
{
    namespace
    {
        using ::ananas::UI::TableColumns;

        constexpr auto ColumnFlags = juce::TableHeaderComponent::visible
                                     | juce::TableHeaderComponent::resizable
                                     | juce::TableHeaderComponent::appearsOnColumnMenu;

        const TableColumns::ColumnHeader SpeakerColumn{1, "Speaker", 70, 30, -1, ColumnFlags, juce::Justification::centred};
        const TableColumns::ColumnHeader XColumn{2, "x (m)", 80, 30, -1, ColumnFlags, juce::Justification::centredRight};
        const TableColumns::ColumnHeader YColumn{3, "y (m)", 80, 30, -1, ColumnFlags, juce::Justification::centredRight};
        const TableColumns::ColumnHeader ModuleColumn{4, "Module", 130, 30, -1, ColumnFlags, juce::Justification::centredLeft};
        const TableColumns::ColumnHeader OutputColumn{5, "Output", 70, 30, -1, ColumnFlags, juce::Justification::centred};
        const TableColumns::ColumnHeader ProblemsColumn{6, "Problems", 400, 30, -1, ColumnFlags, juce::Justification::centredLeft};

        constexpr auto Info = static_cast<int>(LayoutProblem::Severity::info);
        constexpr auto Warning = static_cast<int>(LayoutProblem::Severity::warning);
    }

    SpeakerTableComponent::SpeakerTableComponent(juce::ValueTree &dynamicTree)
        : speakerLayoutNode(dynamicTree.getChildWithName(Identifiers::SpeakerLayoutTreeType))
    {
        addAndMakeVisible(title);
        addAndMakeVisible(table);
        addAndMakeVisible(moduleProblemsLabel);

        title.setFont(juce::Font{juce::FontOptions{18.f, juce::Font::bold}});
        title.setJustificationType(juce::Justification::centredLeft);
        title.setText("Speakers", juce::dontSendNotification);

        moduleProblemsLabel.setJustificationType(juce::Justification::topLeft);
        moduleProblemsLabel.setColour(juce::Label::textColourId, juce::Colours::darkorange.darker(.3f));

        speakerLayoutNode.addListener(this);
        update();
    }

    SpeakerTableComponent::~SpeakerTableComponent()
    {
        speakerLayoutNode.removeListener(this);
        cancelPendingUpdate();
    }

    void SpeakerTableComponent::resized()
    {
        auto bounds = getLocalBounds().reduced(10);
        title.setBounds(bounds.removeFromTop(30));
        moduleProblemsLabel.setBounds(bounds.removeFromBottom(moduleProblemsLabel.getText().isEmpty() ? 0 : 80));
        table.setBounds(bounds);
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
        std::vector<Table::Row> rows;

        for (const auto &speaker: speakerLayoutNode) {
            if (!speaker.hasType(Identifiers::SpeakerTreeType)) continue;

            Table::Row row;
            row.index = speaker[Identifiers::SpeakerIndexPropertyID];
            row.x = speaker[Identifiers::SpeakerXPropertyID];
            row.y = speaker[Identifiers::SpeakerYPropertyID];
            row.moduleIp = speaker[Identifiers::SpeakerModuleIpPropertyID].toString();
            row.output = speaker[Identifiers::SpeakerOutputPropertyID];
            row.severity = speaker[Identifiers::SpeakerSeverityPropertyID];
            row.problems = juce::StringArray::fromLines(speaker[Identifiers::SpeakerProblemsPropertyID].toString());
            row.problems.removeEmptyStrings();
            rows.push_back(std::move(row));
        }

        table.setRows(std::move(rows));

        const auto moduleProblems = speakerLayoutNode[Identifiers::SpeakerLayoutModuleProblemsPropertyID].toString();
        if (moduleProblems != moduleProblemsLabel.getText()) {
            moduleProblemsLabel.setText(moduleProblems, juce::dontSendNotification);
            moduleProblemsLabel.setTooltip(moduleProblems);
            resized();
        }
    }

    //==========================================================================

    SpeakerTableComponent::Table::Table()
    {
        addAndMakeVisible(table);

        addColumn(SpeakerColumn);
        addColumn(XColumn);
        addColumn(YColumn);
        addColumn(ModuleColumn);
        addColumn(OutputColumn);
        addColumn(ProblemsColumn);

        table.setModel(this);
        table.setOutlineThickness(1);
    }

    void SpeakerTableComponent::Table::setRows(std::vector<Row> newRows)
    {
        rows = std::move(newRows);
        table.updateContent();
        table.repaint();
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
            g.fillAll(findColour(AnanasNetworkTable::warningColourId).withAlpha(.25f));
        }
    }

    void SpeakerTableComponent::Table::paintCell(juce::Graphics &g, const int rowNumber, const int columnId,
                                                 const int width, const int height, bool)
    {
        if (rowNumber >= getNumRows()) return;

        const auto &row = rows[static_cast<size_t>(rowNumber)];
        juce::String text;

        switch (columnId) {
            case 1: text = juce::String{row.index + 1};
                break;
            case 2: text = juce::String{row.x, 3};
                break;
            case 3: text = juce::String{row.y, 3};
                break;
            case 4: text = row.moduleIp.isEmpty() ? juce::String{"-"} : row.moduleIp;
                break;
            case 5: text = row.output < 0 ? juce::String{"-"} : juce::String{row.output};
                break;
            case 6:
                if (!row.problems.isEmpty()) {
                    text = row.problems[0];
                    if (row.problems.size() > 1) {
                        text << " (+" << (row.problems.size() - 1) << " more)";
                    }
                }
                break;
            default: break;
        }

        const auto isInfoOnly = columnId == 6 && row.severity == Info;
        g.setColour(findColour(AnanasNetworkTable::textColourId).withAlpha(isInfoOnly ? .5f : 1.f));
        g.setFont(juce::Font{juce::FontOptions{14.f}});
        g.drawText(text, 2, 0, width - 4, height, getJustification(columnId), true);
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
}
