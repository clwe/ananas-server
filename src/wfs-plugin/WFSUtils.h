#ifndef WFSUTILS_H
#define WFSUTILS_H

#include <limits.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <AnanasUtils.h>

namespace ananas::WFS
{
    enum CommandIDs
    {
        SwitchToWfsTab = 0x1001,
        SwitchToNetworkTab = 0x1002,
        ToggleModuleSelectorDisplay = 0x1003,
        RevealModuleSelector = 0x1004
    };

    class Constants
    {
    public:
        constexpr static size_t NumSources{NUM_SOURCES};
        constexpr static size_t MaxNumModules{16};
        // TODO: receive min/max y-coordinates from clients?
        constexpr static int MaxYMetres{10};
        constexpr static int MinYMetres{-3};

        constexpr static int WFSMessengerThreadTimeout{1000};
    };

    enum class SourcePositionAxis : char
    {
        X = 'x',
        Y = 'y'
    };

    class Params
    {
    public:
        struct Param
        {
            juce::StringRef id;
            juce::StringRef name;
        };

        struct RangedParamInt : Param
        {
            int minValue{0};
            int maxValue{0};
            int defaultValue{0};
        };

        struct RangedParamFloat : Param
        {
            juce::NormalisableRange<float> range;
            juce::NormalisableRange<double> rangeDouble;
            float defaultValue{0.f};
        };

        struct BoolParam : Param
        {
            bool defaultValue{true};
        };

        inline static const RangedParamInt NumModules{
            "numModules",
            "Number of Modules",
            1,
            Constants::MaxNumModules,
            8
        };

        inline static const RangedParamFloat SpeakerSpacing{
            "speakerSpacing",
            "Speaker Spacing (m)",
            {.05f, .3f, .001f},
            {.05, .3, .001},
            .2
        };

        inline static const BoolParam ShowModuleSelectors{
            "showModuleSelectors",
            "Show module selectors",
            true
        };

        constexpr static float VirtualSourceDefaultX{.5f};
        constexpr static float VirtualSourceDefaultY{.5f};
        inline static const juce::NormalisableRange<float> VirtualSourcePositionRange{-1.f, 1.f, 1e-6f};

        static juce::String getVirtualSourcePositionParamID(const uint index, SourcePositionAxis axis)
        {
            return "/vs/" + juce::String{index} + "/" + static_cast<char>(axis);
        }

        static juce::String getVirtualSourcePositionParamName(const uint index, SourcePositionAxis axis)
        {
            return "Virtual source " + juce::String{index + 1} + " " + static_cast<char>(axis);
        }

        static juce::String getSecondarySourcePositionParamID(const uint index, SourcePositionAxis axis)
        {
            return "/ss/" + juce::String{index} + "/" + static_cast<char>(axis);
        }

        static float getVirtualSourceDefaultX(const uint sourceIndex)
        {
            return -1.f + (2.f * (static_cast<float>(sourceIndex) + VirtualSourceDefaultX) / Constants::NumSources);
        }
    };

    class Strings
    {
    public:
        inline static const juce::StringRef WfsTabName{"WFS Control"};
        inline static const juce::StringRef NetworkTabName{"Network Overview"};

        inline static const juce::StringRef TabsCommandCategoryName{"Tabs"};
        inline static const juce::StringRef SwitchToWfsTabShortName{"WFS Control tab"};
        inline static const juce::StringRef SwitchToWfsTabDescription{"Switches to the WFS Control tab"};
        inline static const juce::StringRef SwitchToNetworkTabShortName{"Network overview tab"};
        inline static const juce::StringRef SwitchToNetworkTabDescription{"Switches to the Network Overview tab"};

        inline static const juce::StringRef OptionsCommandCategoryName{"Options"};
        inline static const juce::StringRef ToggleModuleSelectorsShortName{"Toggle module selectors"};
        inline static const juce::StringRef ToggleModuleSelectorsDescription{"Toggles display of module selector lists"};

        inline static const juce::StringRef ModulesCommandCategoryName{"Modules"};
        static juce::String getRevealModuleSelectorShortName(const int moduleID) { return "Module " + juce::String{moduleID}; }
        static juce::String getRevealModuleSelectorDescription(const int moduleID) { return "Reveal module selector " + juce::String{moduleID}; }

        inline static const juce::StringRef HideSourceText{"Hide this source"};
        inline static const juce::StringRef HideAllOtherSourcesText{"Hide all except this source"};
    };

    class Identifiers
    {
    public:
        inline static const juce::Identifier StaticTreeType{"WfsParameters"};
    };

    class MenuItems
    {
    public:
        struct MenuItem
        {
            int id{0};
            juce::StringRef text;
        };

        constexpr static int HideSourceMenuID{1};
        constexpr static int HideOtherSourcesMenuID{2};
        constexpr static int ShowAllSourcesMenuID{INT_MAX};

        inline static const MenuItem HideSource{HideSourceMenuID, "Hide this Source"};
        inline static const MenuItem HideOtherSources{HideOtherSourcesMenuID, "Hide all except this Source"};

        inline static const juce::StringRef ShowSourceHeader{"Show source"};
        inline static const MenuItem ShowAllSources{ShowAllSourcesMenuID, "Show all sources"};
    };

    class Utils
    {
    public:
        static juce::String normalisedPointToCoordinateMetres(const juce::Point<float> p, const juce::Point<float> min, const juce::Point<float> max)
        {
            return "(" +
                   juce::String{p.x * max.x, 3} +
                   " m, " +
                   juce::String{p.y < 0 ? -p.y * min.y : p.y * max.y, 3} +
                   " m)";
        }
    };

    class Sockets
    {
    public:
        inline static const ananas::Utils::SenderThreadSocketParams VirtualSourceMessengerSocketParams{
            "Virtual Source Messenger",
            100,
            "224.4.224.5",
            49162,
            49162
        };

        static constexpr uint16_t SecondarySourceMessengerLocalPort{49163};
        static constexpr uint16_t SecondarySourceMessengerRemotePort{49163};
    };

    namespace UI
    {
        class Dimensions
        {
        public:
            constexpr static int SpeakerSpacingSectionHeight{50};

            constexpr static float NodeDiameter{40.f};
            constexpr static float NodeHalfDiameter{NodeDiameter / 2.f};

            constexpr static int ModuleSelectorHeight{25};
            constexpr static int SpeakerIconHeight{25};
        };

        class Shortcuts
        {
        public:
            constexpr static int SwitchToWfsTabKeycode{'['};
            constexpr static int SwitchToNetworkTabKeycode{']'};
            constexpr static int ToggleModuleSelectorsKeycode{'m'};
            constexpr static int RevealModuleSelectorListKeycode{'1'};
        };
    }
}

#endif //WFSUTILS_H
