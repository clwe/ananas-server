#include "ClientsOverviewComponent.h"
#include <AnanasLookAndFeel.h>
#include <AnanasUtils.h>

namespace ananas::UI
{
    ClientsOverviewComponent::ClientsOverviewComponent(juce::ValueTree &dynamicTree)
        : clientTable(std::make_unique<ClientTable>()),
          tree(dynamicTree)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(rebootAllClientsButton);
        addAndMakeVisible(overviewPanel);
        addAndMakeVisible(clientTable.get());

        title.setFont(juce::Font{juce::FontOptions{18.f, juce::Font::bold}});
        title.setJustificationType(juce::Justification::centredLeft);
        title.setText(Strings::ClientsSectionTitle, juce::dontSendNotification);

        rebootAllClientsButton.setButtonText(Strings::RebootAllClientsButtonText);
        rebootAllClientsButton.setTooltip(Strings::RebootAllClientsButtonTooltip);
        rebootAllClientsButton.onClick = [this]
        {
            triggerClientReboot();
        };

        tree.addListener(this);
    }

    ClientsOverviewComponent::~ClientsOverviewComponent()
    {
        tree.removeListener(this);
    }

    void ClientsOverviewComponent::update(const juce::var &var)
    {
        if (!isVisible()) return;

        clientTable->update(var);
        overviewPanel.update(var);
    }

    void ClientsOverviewComponent::paint(juce::Graphics &g)
    {
        g.fillAll(findColour(backgroundColourId));
    }

    void ClientsOverviewComponent::resized()
    {
        auto bounds{getLocalBounds()};
        auto titleRow{
            bounds.removeFromTop(Dimensions::NetworkSectionTitleHeight)
            .reduced(6, 0)
        };
        title.setBounds(titleRow.removeFromLeft(85));
        rebootAllClientsButton.setBounds(titleRow.removeFromLeft(100).reduced(8));
        overviewPanel.setBounds(bounds.removeFromTop(45));
        clientTable->setBounds(bounds);
    }

    void ClientsOverviewComponent::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property)
    {
        if (!isVisible()) return;

        if (property == Utils::Identifiers::ConnectedClientsParamID) {
            update(treeWhosePropertyHasChanged[property]);
            handleAsyncUpdate();
        }
    }

    void ClientsOverviewComponent::handleAsyncUpdate()
    {
        repaint();
    }

    void ClientsOverviewComponent::triggerClientReboot() const
    {
        tree.setProperty(Utils::Identifiers::ClientsShouldRebootParamID, true, nullptr);
    }

    //==========================================================================

    ClientsOverviewComponent::OverviewPanel::OverviewPanel()
    {
        addAndMakeVisible(totalClientsLabel);
        addAndMakeVisible(totalClientsValue);
        addAndMakeVisible(presentationTimeIntervalLabel);
        addAndMakeVisible(presentationTimeIntervalValue);

        totalClientsLabel.setText(Strings::TotalClientsLabel, juce::dontSendNotification);
        presentationTimeIntervalLabel.setText(Strings::PresentationTimeIntervalLabel, juce::dontSendNotification);
    }

    juce::String ClientsOverviewComponent::OverviewPanel::getPresentationTimeIntervalText(const int presentationTimeInterval)
    {
        return presentationTimeInterval < 1000
                   ? juce::String{presentationTimeInterval} + " ns"
                   : Strings::formatWithThousandsSeparator(static_cast<float>(presentationTimeInterval) / 1000.f) + " µs";
    }

    void ClientsOverviewComponent::OverviewPanel::update(const juce::var &var)
    {
        if (!isVisible()) return;

        if (auto *obj = var.getDynamicObject()) {
            const auto &props{obj->getProperties()};
            totalClientsValue.setText(juce::String{props.size()}, juce::dontSendNotification);

            juce::int32 minOffsetTime{INT32_MAX}, maxOffsetTime{INT32_MIN};
            for (const auto &prop: props) {
                if (const auto *client = prop.value.getDynamicObject()) {
                    const juce::int32 presentationTimeOffset{
                                client->getProperty(Utils::Identifiers::ClientPresentationTimeOffsetNsPropertyID)
                            },
                            audioPTPOffset{
                                client->getProperty(Utils::Identifiers::ClientAudioPTPOffsetPropertyID)
                            };
                    const auto offsetTime{presentationTimeOffset + audioPTPOffset};
                    if (offsetTime < minOffsetTime) {
                        minOffsetTime = offsetTime;
                    }
                    if (offsetTime > maxOffsetTime) {
                        maxOffsetTime = offsetTime;
                    }
                }
            }

            const auto presentationTimeInterval{maxOffsetTime - minOffsetTime};

            if (++numUpdates >= 250) {
                maxPresentationTimeInterval = 0;
                numUpdates = 0;
            }

            auto maxNanoseconds{5000.f};
            if (const auto lnf{dynamic_cast<AnanasLookAndFeel *>(&getLookAndFeel())}) {
                maxNanoseconds = lnf->getMaxMicrosecondsToDisplay() * 1000.f;
            }

            if (presentationTimeInterval > maxPresentationTimeInterval) {
                maxPresentationTimeInterval = presentationTimeInterval;
                presentationTimeIntervalValue.setMax(
                    juce::jmin(1.f, static_cast<float>(maxPresentationTimeInterval) / maxNanoseconds)
                );
            }

            presentationTimeIntervalValue.setText(
                getPresentationTimeIntervalText(presentationTimeInterval),
                juce::dontSendNotification
            );

            presentationTimeIntervalValue.setBackgroundWidth(
                juce::jlimit(
                    0.f,
                    1.f,
                    static_cast<float>(presentationTimeInterval) / maxNanoseconds
                )
            );
        }
    }

    void ClientsOverviewComponent::OverviewPanel::paint(juce::Graphics &g)
    {
        g.fillAll(juce::Colours::transparentWhite);
    }

    void ClientsOverviewComponent::OverviewPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(10);

        // Left stat
        const auto leftBounds = bounds.removeFromLeft(bounds.getWidth() / 4);
        juce::FlexBox leftFlex;
        leftFlex.flexDirection = juce::FlexBox::Direction::row;
        leftFlex.items.add(juce::FlexItem(totalClientsLabel).withWidth(90));
        leftFlex.items.add(juce::FlexItem(totalClientsValue).withFlex(1));
        leftFlex.performLayout(leftBounds);

        // Right stat
        juce::FlexBox rightFlex;
        rightFlex.flexDirection = juce::FlexBox::Direction::row;
        rightFlex.items.add(juce::FlexItem(presentationTimeIntervalLabel).withWidth(200));
        rightFlex.items.add(juce::FlexItem(presentationTimeIntervalValue).withFlex(1));
        rightFlex.performLayout(bounds);
    }

    //==========================================================================

    void ClientsOverviewComponent::OverviewPanel::PresentationTimeInterval::setBackgroundWidth(const float proportion)
    {
        backgroundProportion = proportion;
        repaint();
    }

    void ClientsOverviewComponent::OverviewPanel::PresentationTimeInterval::setMax(const float maxSeen)
    {
        maxValue = maxSeen;
    }

    void ClientsOverviewComponent::OverviewPanel::PresentationTimeInterval::paint(juce::Graphics &g)
    {
        if (auto *lnf{dynamic_cast<AnanasLookAndFeel *>(&getLookAndFeel())}) {
            lnf->drawPresentationTimeInterval(g, *this, backgroundProportion, maxValue);
        }

        Label::paint(g);
    }

    //==========================================================================

    ClientsOverviewComponent::ClientTable::ClientTable()
    {
        addAndMakeVisible(table);

        addColumn(TableColumns::ClientTableIpAddress);
        addColumn(TableColumns::ClientTableSerialNumber);
        addColumn(TableColumns::ClientTableFirmwareTypeVersion);
        addColumn(TableColumns::ClientTablePTPLock);
        addColumn(TableColumns::ClientTablePresentationTimeOffset);
        addColumn(TableColumns::ClientTableBufferFillPercent);
        addColumn(TableColumns::ClientTableSamplingRate);
        addColumn(TableColumns::ClientTablePercentCPU);
        addColumn(TableColumns::ClientTableSecondarySourceCoordinates);
        addColumn(TableColumns::ClientTableSourcesSpeakers);

        table.setModel(this);
        table.setOutlineThickness(1);
    }

    void ClientsOverviewComponent::ClientTable::update(const juce::var &clientInfo)
    {
        if (!isVisible()) return;

        rows.clear();

        if (auto *obj = clientInfo.getDynamicObject()) {
            for (const auto &prop: obj->getProperties()) {
                Row row;
                row.ip = prop.name.toString();

                if (const auto *client = prop.value.getDynamicObject()) {
                    row.serialNumber = client->getProperty(Utils::Identifiers::ClientSerialNumberPropertyID).toString();
                    row.firmwareTypeVersion = client->getProperty(Utils::Identifiers::ClientFirmwareTypeVersionPropertyID);
                    row.ptpLock = client->getProperty(Utils::Identifiers::ClientPTPLockPropertyID);
                    row.presentationTimeOffsetNs = client->getProperty(Utils::Identifiers::ClientPresentationTimeOffsetNsPropertyID);
                    row.presentationTimeOffsetFrame = client->getProperty(Utils::Identifiers::ClientPresentationTimeOffsetFramePropertyID);
                    row.audioPTPOffsetNs = client->getProperty(Utils::Identifiers::ClientAudioPTPOffsetPropertyID);
                    row.bufferFillPercent = client->getProperty(Utils::Identifiers::ClientBufferFillPercentPropertyID);
                    row.samplingRate = client->getProperty(Utils::Identifiers::ClientSamplingRatePropertyID);
                    row.percentCPU = client->getProperty(Utils::Identifiers::ClientPercentCPUPropertyID);
                    row.secondarySourceCoordinates = client->getProperty(Utils::Identifiers::ClientSecondarySourceCoordinatesPropertyID);
                    row.sourcesSpeakers = client->getProperty(Utils::Identifiers::ClientSourcesSpeakersPropertyID).toString();
                }

                rows.add(row);
            }
        }

        table.updateContent();
        repaint();
    }

    int ClientsOverviewComponent::ClientTable::getNumRows()
    {
        return rows.size();
    }

    void ClientsOverviewComponent::ClientTable::paintRowBackground(juce::Graphics &g, const int rowNumber, int width, int height, const bool rowIsSelected)
    {
        juce::ignoreUnused(width, height);

        if (rowIsSelected)
            g.fillAll(juce::Colours::lightblue);
        else if (rowNumber % 2 == 0)
            g.fillAll(juce::Colour(0xffeeeeee));
        else
            g.fillAll(juce::Colours::white);
    }

    void ClientsOverviewComponent::ClientTable::paintCell(juce::Graphics &g,
                                                          const int rowNumber,
                                                          const int columnId,
                                                          const int width,
                                                          const int height,
                                                          bool rowIsSelected)
    {
        juce::ignoreUnused(rowIsSelected);

        if (rowNumber < rows.size()) {
            const auto &[
                ip,
                serialNumber,
                firmwareTypeVersion,
                ptpLock,
                offsetTime,
                offsetFrame,
                audioPTPOffset,
                bufferFillPercent,
                samplingRate,
                percentCPU,
                secondarySourceCoordinates,
                sourcesSpeakers
            ] = rows[rowNumber];
            juce::String text;

            switch (columnId) {
                case 1: text = ip;
                    break;
                case 2: text = serialNumber;
                    break;
                case 3: text = firmwareTypeVersion;
                    break;
                case 4: text = ptpLock ? "Yes" : "No";
                    g.setColour(ptpLock ? findColour(okColourId) : findColour(warningColourId));
                    g.fillRect(2, 2, width - 4, height - 4);
                    break;
                case 5: text = Strings::formatWithThousandsSeparator(offsetTime + audioPTPOffset) +
                               " (" + juce::String(offsetFrame) +
                               (offsetFrame == 1 ? " frame)" : " frames)");
                    break;
                case 6: text = juce::String(bufferFillPercent) + " %";
                    g.setColour(bufferFillPercent > 80 || bufferFillPercent < 20 ? findColour(warningColourId) : findColour(okColourId));
                    g.fillRect(2, 2, static_cast<int>((width - 4) * (bufferFillPercent / 100.f)), height - 4);
                    break;
                case 7: text = Strings::formatWithThousandsSeparator(samplingRate, 6);
                    break;
                case 8: text = juce::String{percentCPU, 3};
                    break;
                case 9: text = secondarySourceCoordinates;
                    break;
                case 10: text = sourcesSpeakers;
                    break;
                default: break;
            }

            g.setColour(findColour(textColourId));
            g.setFont(juce::Font{juce::FontOptions{14.f}});
            g.drawText(text, 2, 0, width - 4, height, getJustification(columnId), true);
        }
    }

    void ClientsOverviewComponent::ClientTable::resized()
    {
        table.setBounds(getLocalBounds().reduced(10));
    }
}
