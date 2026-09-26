set(companyName "Emeraude")

set(companyCode "EMRD")

set(pluginCode Awfs)

set(bundleID "org.emeraude.ananas-wfs")

set(juceFormats Standalone)

# Up to 22 sources @ 32 frames
# Up to 44 sources @ 16 frames
set(numSources 16)

juce_add_plugin(ananasWFS
        COMPANY_NAME "${companyName}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE "${companyCode}"
        PLUGIN_CODE ${pluginCode}
        FORMATS ${juceFormats}
        PRODUCT_NAME "AnanasWFS"
)

clap_juce_extensions_plugin(
        TARGET ananasWFS
        CLAP_ID "${bundleID}"
        CLAP_FEATURES utility
)

target_compile_definitions(ananasWFS
        PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        NUM_SOURCES=${numSources}
        SHOW_NO_NETWORK_OVERLAY=$<BOOL:${SHOW_NO_NETWORK_OVERLAY}>
)

target_sources(ananasWFS
        PRIVATE
        ArrayLayout.cpp
        PluginEditor.cpp
        PluginProcessor.cpp
        SecondarySourceMessenger.cpp
        VirtualSourceMessenger.cpp
        UI/WFSInterfaceComponent.cpp
        UI/XYControllerComponent.cpp
        UI/SpeakerIconComponent.cpp
        UI/ModuleComponent.cpp
        UI/WfsLookAndFeel.cpp
)

target_link_libraries(ananasWFS
        PRIVATE
        juce::juce_core
        juce::juce_osc
        juce::juce_audio_devices
        juce::juce_audio_utils
        clap_juce_extensions
        ananas_utilities
        ananas_ui
        ananas_server
        ananas_network_overview
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
