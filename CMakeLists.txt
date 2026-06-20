cmake_minimum_required(VERSION 3.16)
project(MyGame VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_STATIC_LIBRARIES TRUE)

# Where your static SFML 3 lives. Override on the command line without editing
# this file:  cmake -B build -DSFML_DIR=/path/to/SFML/lib/cmake/SFML
if(NOT DEFINED SFML_DIR)
    set(SFML_DIR "D:/libs/SFML-3.1.0/lib/cmake/SFML")
endif()

add_executable(MyGame
    src/main.cpp
    src/Simulation.cpp
    src/Brain.cpp
    src/BrainView.cpp
    src/Champions.cpp
    src/Charts.cpp
    src/Headless.cpp
    src/Leaderboard.cpp
    src/SaveState.cpp
    src/SliderPanel.cpp
    src/Timeline.cpp
)

find_package(SFML 3 COMPONENTS System Window Graphics Audio Network REQUIRED)

target_link_libraries(MyGame
    PRIVATE
        SFML::System
        SFML::Window
        SFML::Graphics
        SFML::Audio
        SFML::Network
)

if (WIN32)
    # native save/open file dialogs (SaveState.cpp)
    target_link_libraries(MyGame PRIVATE comdlg32)
endif()
