add_library(ananas_server
        Server.cpp
        Fifo.cpp
        Packet.cpp
        ClientInfo.cpp
        AuthorityInfo.cpp
        SwitchInfo.cpp
)

set_target_properties(ananas_server PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        VERSION 0.4.0
        SOVERSION 1
)

target_include_directories(ananas_server
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ananas_server
        PUBLIC
        juce::juce_audio_basics
        ananas_utilities
        $<$<PLATFORM_ID:Linux>:atomic>
)
