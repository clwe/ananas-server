juce_add_console_app(ananas_tests
        PRODUCT_NAME "Ananas Tests")

target_sources(ananas_tests
        PRIVATE
        Main.cpp
        ArrayLayoutTests.cpp
        ModuleListTests.cpp
        ${CMAKE_SOURCE_DIR}/src/renderer-plugin/ArrayLayout.cpp
)

target_include_directories(ananas_tests
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src/renderer-plugin
)

target_compile_definitions(ananas_tests
        PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        NUM_SOURCES=8
)

target_link_libraries(ananas_tests
        PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
        juce::juce_data_structures
        ananas_server
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
