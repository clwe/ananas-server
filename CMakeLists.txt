cmake_minimum_required(VERSION 3.30)

project(ananas VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)

juce_add_binary_data(fonts
        HEADER_NAME AnanasFonts.h
        SOURCES
        "resources/fonts/FF Unit Pro Regular.otf"
        "resources/fonts/FF Unit Pro Bold.otf"
)

add_subdirectory(lib/ananas-utilities)

add_subdirectory(lib/ananas-server)

add_subdirectory(src/console-app)

option(SHOW_NO_NETWORK_OVERLAY
        "Show UI overlay if a network connection cannot be established"
        ON)

add_subdirectory(lib/ananas-ui)

add_subdirectory(lib/network-ui)

add_subdirectory(src/ananas-server-plugin)

add_subdirectory(src/wfs-plugin)
