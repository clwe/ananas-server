#ifndef ANANASUIUTILS_H
#define ANANASUIUTILS_H

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ananas::UI
{
    class Dimensions
    {
    public:
        constexpr static int UiWidth{1200};
        constexpr static int UiHeight{900};

        constexpr static int SwitchesSectionHeight{175};
        constexpr static int TimeAuthoritySectionHeight{112};
        constexpr static int NetworkSectionTitleHeight{40};
    };

    class Timing
    {
    public:
        constexpr static int TooltipDelayTimeMs{500};
    };

    class Strings
    {
    public:
        inline static const juce::StringRef SwitchesSectionTitle{"SWITCHES"};
        inline static const juce::StringRef AddSwitchButtonName{"Add switch button"};
        inline static const juce::StringRef AddSwitchButtonText{"+"};
        inline static const juce::StringRef AddSwitchButtonTooltip{"Click to add a new switch."};
        inline static const juce::StringRef ResetSwitchButtonText{"↻"};
        inline static const juce::StringRef RemoveSwitchButtonText{"×"};

        inline static const juce::StringRef TimeAuthoritySectionTitle{"TIME AUTHORITY"};

        inline static const juce::StringRef ClientsSectionTitle{"CLIENTS"};
        inline static const juce::StringRef RebootAllClientsButtonText{"Reboot all"};
        inline static const juce::StringRef RebootAllClientsButtonTooltip{"Click to send a reboot instruction to all clients."};
        inline static const juce::StringRef TotalClientsLabel{"Total clients: "};
        inline static const juce::StringRef PresentationTimeIntervalLabel{"Approx. group asynchronicity: "};

        inline static const juce::String OverlayInitialText{"Looking for network..."};
        inline static const juce::String OverlayNoNetworkText{"No network connection."};

        static juce::String formatWithThousandsSeparator(const int value)
        {
            std::stringstream ss;
            ss.imbue(std::locale("en_GB.UTF-8"));
            ss << value;
            return ss.str();
        }

        static juce::String formatWithThousandsSeparator(const float value, const int decimals = 3)
        {
            std::stringstream ss;
            ss.imbue(std::locale("en_GB.UTF-8"));
            ss << std::fixed << std::setprecision(decimals) << value;
            return ss.str();
        }
    };

    class TableColumns
    {
    public:
        struct ColumnHeader
        {
            int id{};
            juce::StringRef label;
            int width{};
            int minWidth{30};
            int maxWidth{-1};
            int flags{juce::TableHeaderComponent::defaultFlags};
            juce::Justification::Flags justification{juce::Justification::centredLeft};
        };

        inline static const ColumnHeader ClientTableIpAddress{
            1, "IP address", 125, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centredLeft
        };
        inline static const ColumnHeader ClientTableSerialNumber{
            2, "Serial number", 100, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader ClientTableFirmwareTypeVersion{
            3, "Firmware", 125, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader ClientTablePTPLock{
            4, "PTP lock", 75, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader ClientTablePresentationTimeOffset{
            5, "Presentation offset (ns)", 160, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centredRight
        };
        inline static const ColumnHeader ClientTableBufferFillPercent{
            6, "Buffer fill level", 180, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader ClientTableSamplingRate{
            7, "Sampling rate (Hz)", 130, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader ClientTablePercentCPU{
            8, "CPU %", 60, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centredRight
        };
        inline static const ColumnHeader ClientTableSecondarySourceCoordinates{
            9, "Speaker Coordinates (m)", 160, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader ClientTableSourcesSpeakers{
            10, "Sources / speakers", 130, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };

        inline static const ColumnHeader AuthorityTableIpAddress{
            1, "IP address", 125, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centredLeft
        };
        inline static const ColumnHeader AuthorityTableSerialNumber{
            2, "Serial number", 100, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };
        inline static const ColumnHeader AuthorityTableFeedbackAccumulator{
            3, "USB feedback accumulator", 200, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centredRight
        };
        inline static const ColumnHeader AuthorityTableSamplingRate{
            4, "USB audio sampling rate (Hz)", 200, 30, -1,
            juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::appearsOnColumnMenu,
            juce::Justification::centred
        };

        inline static const ColumnHeader SwitchesTableIpAddress{
            1, "IP address", 150, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centredLeft
        };
        inline static const ColumnHeader SwitchesTableUsername{
            2, "Username", 100, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centredLeft
        };
        inline static const ColumnHeader SwitchesTablePassword{
            3, "Password", 100, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centredLeft
        };
        inline static const ColumnHeader SwitchesTableDrift{
            4, "Freq. drift (ppb)", 100, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centredRight
        };
        inline static const ColumnHeader SwitchesTableOffset{
            5, "Offset (ns)", 100, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centredRight
        };
        inline static const ColumnHeader SwitchesTableResetPTP{
            6, "Reset PTP", 75, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centred
        };
        inline static const ColumnHeader SwitchesTableRemoveSwitch{
            7, "Remove", 75, 30, -1,
            juce::TableHeaderComponent::notSortable,
            juce::Justification::centred
        };
    };
}

#endif //ANANASUIUTILS_H
