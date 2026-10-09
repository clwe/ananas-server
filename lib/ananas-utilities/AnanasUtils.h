#ifndef ANANASUTILS_H
#define ANANASUTILS_H

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace ananas
{
    class Utils
    {
    public:
        struct VersionNumber
        {
            uint8_t major;
            uint8_t minor;
            uint8_t revision;
        };

        enum class FirmwareType : juce::uint8
        {
            client = 0,
            wfsModule = 1,
            ambisonicsModule,
            passthrough
        };

        static juce::String FirmwareTypeToString(const FirmwareType type)
        {
            switch (type) {
                case FirmwareType::client: return "Client";
                case FirmwareType::wfsModule: return "WFS";
                case FirmwareType::ambisonicsModule: return "Ambisonics";
                case FirmwareType::passthrough: return "Passthrough";
                default: return "Unknown";
            }
        }

        static juce::String VersionNumberToString(const VersionNumber version)
        {
            return juce::String{version.major} + "." + juce::String{version.minor} + "." + juce::String{version.revision};
        }

        class Constants
        {
        public:
            constexpr static juce::int32 AuthorityInitialUSBFeedbackAccumulator{48 << 25};
            // Assumed for modules that don't report their source/speaker counts.
            constexpr static int LegacyNumSources{16};
            constexpr static int LegacyNumSpeakers{2};
        };

        class Strings
        {
        public:
            // TODO: make the local interface IP an option?
            inline const static juce::StringRef LocalInterfaceIP{"192.168.10.10"};

            static juce::String getInputLabel(const size_t channel)
            {
                return InputLabel + juce::String{channel};
            }

            static juce::String getOutputLabel(const size_t channel)
            {
                return OutputLabel + juce::String{channel};
            }

        private:
            inline static const juce::StringRef InputLabel{"Input #"};
            inline static const juce::StringRef OutputLabel{"Output #"};
        };

        class Identifiers
        {
        public:
            inline static const juce::Identifier DynamicTreeType{"EphemeralData"};
            inline static const juce::Identifier PersistentTreeType{"PersistentData"};

            inline const static juce::Identifier SwitchesParamID{"Switches"};
            inline const static juce::Identifier SwitchTreeType{"Switch"};

            inline static const juce::Identifier SwitchIdentifierBase{"switch_"};
            inline const static juce::Identifier SwitchIpPropertyID{"switchIP"};
            inline const static juce::Identifier SwitchUsernamePropertyID{"switchUsername"};
            inline const static juce::Identifier SwitchPasswordPropertyID{"switchPassword"};
            inline const static juce::Identifier SwitchClockIdPropertyId{"clock-id"};
            inline const static juce::Identifier SwitchFreqDriftPropertyId{"freq-drift"};
            inline const static juce::Identifier SwitchGmClockIdPropertyId{"gm-clock-id"};
            inline const static juce::Identifier SwitchGmPriority1PropertyId{"gm-priority1"};
            inline const static juce::Identifier SwitchGmPriority2PropertyId{"gm-priority2"};
            inline const static juce::Identifier SwitchIAmGmPropertyId{"i-am-gm"};
            inline const static juce::Identifier SwitchMasterClockIdPropertyId{"master-clock-id"};
            inline const static juce::Identifier SwitchNamePropertyId{"name"};
            inline const static juce::Identifier SwitchOffsetPropertyId{"offset"};
            inline const static juce::Identifier SwitchPriority1PropertyId{"priority1"};
            inline const static juce::Identifier SwitchPriority2PropertyId{"priority2"};
            inline const static juce::Identifier SwitchSlavePortPropertyId{"slave-port"};
            inline const static juce::Identifier SwitchSlavePortDelayPropertyId{"slave-port-delay"};

            inline static const juce::Identifier TimeAuthorityParamID{"TimeAuthority"};

            inline const static juce::Identifier AuthorityIpPropertyID{"ipAddress"};
            inline const static juce::Identifier AuthoritySerialNumberPropertyID{"serialNumber"};
            inline const static juce::Identifier AuthorityFeedbackAccumulatorPropertyID{"feedbackAccumulator"};

            inline static const juce::Identifier ConnectedClientsParamID{"ConnectedClients"};
            inline static const juce::Identifier ClientTreeType{"Client"};

            inline const static juce::Identifier ClientSerialNumberPropertyID{"serialNumber"};
            inline const static juce::Identifier ClientFirmwareTypeVersionPropertyID{"firmwareTypeVersion"};
            inline const static juce::Identifier ClientPTPLockPropertyID{"ptpLock"};
            inline const static juce::Identifier ClientPresentationTimeOffsetNsPropertyID{"presentationTimeOffsetNs"};
            inline const static juce::Identifier ClientPresentationTimeOffsetFramePropertyID{"presentationTimeOffsetFrame"};
            inline const static juce::Identifier ClientAudioPTPOffsetPropertyID{"AudioPTPOffsetNs"};
            inline const static juce::Identifier ClientBufferFillPercentPropertyID{"bufferFillPercent"};
            inline const static juce::Identifier ClientSamplingRatePropertyID{"samplingRate"};
            inline const static juce::Identifier ClientPercentCPUPropertyID{"percentCPU"};
            inline const static juce::Identifier ClientSecondarySourceCoordinatesPropertyID{"secondarySourceCoordinates"};
            inline const static juce::Identifier ClientSourcesSpeakersPropertyID{"sourcesSpeakers"};

            inline const static juce::Identifier ModulesParamID{"Modules"};
            inline const static juce::Identifier ModuleTreeType{"Module"};

            inline const static juce::Identifier ModuleSlotPropertyID{"ModuleSlot"};
            inline const static juce::Identifier ModuleFirmwareTypePropertyID{"ModuleFirmwareType"};
            inline const static juce::Identifier ModuleNumSourcesPropertyID{"ModuleNumSources"};
            inline const static juce::Identifier ModuleNumSpeakersPropertyID{"ModuleNumSpeakers"};
            inline const static juce::Identifier ModuleIsConnectedPropertyID{"ModuleIsConnected"};
        };

        struct ThreadParams
        {
            juce::StringRef name;
            int timeoutMs{};
        };

        struct ThreadSocketParams : ThreadParams
        {
            juce::StringRef ip;
            juce::uint16 localPort{};
        };

        struct SenderThreadSocketParams : ThreadSocketParams
        {
            juce::uint16 remotePort{};
        };

        struct ListenerThreadSocketParams : ThreadSocketParams
        {
            int disconnectionThresholdMs{};
        };

        // Make target's children match source's, matching children by the
        // value of the property `propName`. Only what differs is changed, so
        // listeners only hear about real changes.
        static void syncChildren(juce::ValueTree &target, const juce::ValueTree &source, const juce::Identifier &propName) 
        {
            // Children are matched by this property, so every child needs it.
            for (const auto &child: source)
                jassert(child.hasProperty(propName));
            
            for (int i = target.getNumChildren() - 1; i >= 0; --i) {
                const auto targetChild {target.getChild(i)};
                const auto targetPropVal = targetChild.getProperty(propName);
                const auto match = source.getChildWithProperty(propName, targetPropVal);
                if (!match.isValid()) {
                    // targetChild is not in source anymore, remove it
                    target.removeChild(i, nullptr);
                }
            }

            for (int i = 0; i < source.getNumChildren(); ++i) {
                const auto sourceChild = source.getChild(i);
                const auto sourcePropVal = sourceChild.getProperty(propName);
                auto match = target.getChildWithProperty(propName, sourcePropVal);
                if (!match.isValid()) {
                    // sourceChild is not in target yet, add it
                    target.addChild(sourceChild.createCopy(), i, nullptr);
                } else {
                    // sourceChild is in the target already, let's match their properties here
                    for (int j = sourceChild.getNumProperties() - 1; j >= 0; --j) {
                        // iterating through matched childrens properties
                        const auto sourceChildPropName = sourceChild.getPropertyName(j);
                        const auto sourceChildPropVal = sourceChild.getProperty(sourceChildPropName);
                        match.setProperty(sourceChildPropName, sourceChildPropVal, nullptr);
                    }
                    // check for, and remove dangling properties in the target child
                    for (int j = match.getNumProperties() - 1; j >= 0; --j) {
                        const auto name = match.getPropertyName(j);
                        if (!sourceChild.hasProperty(name))
                            match.removeProperty(name, nullptr);
                    }
                }
            }
        }
    };
}

#endif //ANANASUTILS_H
